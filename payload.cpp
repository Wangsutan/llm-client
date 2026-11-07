#include "payload.h"

#include "llm_limits.h"
#include "memprobe.h"

#include "miniz.h"

char PayloadReceiver::s_work[WORK_BUF_SIZE + 1];

// 解压器约 11KB：放栈上会打爆 loopTask 的 8KB 栈（实测 stack canary panic），
// 所以放 .bss 常驻复用，每轮不再 malloc。
static tinfl_decompressor g_inflater;

size_t PayloadReceiver::payloadCap() {
  return WORK_BUF_SIZE - 1024;
}

// 解析 gzip 头/尾：算出 deflate 数据起始偏移与解压后的原始长度（ISIZE）
bool PayloadReceiver::gzipParse(const char *gz, size_t gzLen, size_t *dataOff, size_t *origLen) {
  if (gzLen < 18) {
    return false;
  }
  const uint8_t *p = (const uint8_t *)gz;
  if (p[0] != 0x1f || p[1] != 0x8b || p[2] != 0x08) {
    return false;
  }
  uint8_t flg = p[3];
  size_t off = 10;
  if (flg & 0x04) {
    // FEXTRA：先确认 2 字节 XLEN 与 XLEN 字节的额外字段都落在文件内，再前进。
    // 不靠末尾的 off + 8 >= gzLen 兜底，免得越界的 off 继续参与后续偏移运算。
    if (gzLen - off < 2) return false;
    size_t xlen = (size_t)p[off] | ((size_t)p[off + 1] << 8);
    if (gzLen - off - 2 < xlen) return false;
    off += 2 + xlen;
  }
  if (flg & 0x08) {
    while (off < gzLen && p[off] != 0) off++;
    off++;
  }
  if (flg & 0x10) {
    while (off < gzLen && p[off] != 0) off++;
    off++;
  }
  if (flg & 0x02) {
    off += 2;
  }
  if (off + 8 >= gzLen) {
    return false;
  }
  *dataOff = off;
  *origLen = (size_t)((uint32_t)p[gzLen - 4] | ((uint32_t)p[gzLen - 3] << 8) |
                      ((uint32_t)p[gzLen - 2] << 16) | ((uint32_t)p[gzLen - 1] << 24));
  return *origLen > 0;
}

// 解压 raw deflate（ROM 里的 tinfl），结果写进调用方提供的工作区
bool PayloadReceiver::gzipInflate(const char *gz, size_t gzLen, size_t dataOff, size_t origLen,
                                  char *out, size_t outCap, size_t *outLen) {
  if (origLen > outCap) {
    return false;
  }
  tinfl_init(&g_inflater);
  const uint8_t *p = (const uint8_t *)gz;
  size_t inLen = gzLen - dataOff - 8;  // 去掉 gzip 头（含可选字段）与 8 字节尾
  size_t outSize = origLen;
  tinfl_status st =
      tinfl_decompress(&g_inflater, p + dataOff, &inLen, (mz_uint8 *)out, (mz_uint8 *)out, &outSize,
                       TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  if (st != TINFL_STATUS_DONE) {
    return false;
  }
  out[outSize] = 0;
  *outLen = outSize;
  return true;
}

void PayloadReceiver::releasePlain() {
  workLen_ = 0;
  workReady_ = false;
}

void PayloadReceiver::uploading(WebServer &server) {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    releasePlain();
    memBegin("upload");
    uploadBody_ = "";
    gzipUpload_ = up.filename.endsWith(".gz");
    if (gzipUpload_) {
      uploadBody_.reserve(2048);  // 只有 gzip 路径才用到这个 String
    }
    reject_ = {nullptr, 0, nullptr};
    // 预算在开始时就定好：累积过程中 String 自己也在吃堆，实时算会把自己算进去
    uploadCap_ = payloadCap();
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (reject_.reason) {
      return;  // 已判定拒收：不再继续吃内存
    }
    const size_t need = up.currentSize;
    if (gzipUpload_) {
      // 压缩数据很小（实测为明文的 1.5%-6.5%），仍旧收在 String 里，解压时一并释放
      // 减式比较：have 始终 ≤ 预算，写成 need > cap - have 等价，但不会先做加法
      const size_t have = uploadBody_.length();
      if (uploadCap_ == 0 || need > uploadCap_ - have) {
        Serial.printf("[web] reject upload: gz %u+%u bytes over cap %u\n", (unsigned)have,
                      (unsigned)need, (unsigned)uploadCap_);
        reject_ = {"payload too large", 413, "payload_too_large"};
        uploadBody_ = "";
        return;
      }
      uploadBody_.concat((const char *)up.buf, need);
      return;
    }
    // 明文：直接累积进工作区，省掉中间那份 String。
    // 减式比较：workLen_ 始终 ≤ 预算（每轮开始清零、每块写入前都先判），
    // 等价于先求和再比，但不会先把两个大数相加。
    if (uploadCap_ == 0 || need > uploadCap_ - workLen_) {
      Serial.printf("[web] reject upload: plain %u+%u bytes over cap %u (heap %u)\n",
                    (unsigned)workLen_, (unsigned)need, (unsigned)uploadCap_,
                    (unsigned)ESP.getFreeHeap());
      reject_ = {"payload too large", 413, "payload_too_large"};
      releasePlain();
      return;
    }
    memcpy(s_work + workLen_, up.buf, need);
    workLen_ += need;
  } else if (up.status == UPLOAD_FILE_END) {
    memSample("uploaded");
    if (reject_.reason) {
      return;
    }
    if (!gzipUpload_) {
      // 明文：工作区里已经是完整 JSON，补个结尾 NUL 就能解析
      if (workLen_ == 0) {
        reject_ = {"empty payload", 400, "empty_payload"};
        return;
      }
      s_work[workLen_] = 0;
      workReady_ = true;
      return;
    }
    const char *gz = uploadBody_.c_str();
    const size_t gzLen = uploadBody_.length();
    size_t dataOff = 0;
    size_t origLen = 0;
    if (!gzipParse(gz, gzLen, &dataOff, &origLen)) {
      reject_ = {"bad gzip", 400, "bad_gzip"};
      uploadBody_ = "";
      return;
    }
    const size_t cap = payloadCap();
    if (cap == 0 || origLen > cap) {
      Serial.printf("[web] reject gzip: plain %u bytes over cap %u\n", (unsigned)origLen,
                    (unsigned)cap);
      reject_ = {"payload too large", 413, "payload_too_large"};
      uploadBody_ = "";
      return;
    }
    size_t n = 0;
    const bool ok = gzipInflate(gz, gzLen, dataOff, origLen, s_work, WORK_BUF_SIZE, &n);
    uploadBody_ = "";  // 压缩数据用完即释放
    if (!ok) {
      reject_ = {"bad gzip", 400, "bad_gzip"};
      return;
    }
    workLen_ = n;
    workReady_ = true;
  }
}

bool PayloadReceiver::read(WebServer &server, JsonDocument &in) {
  if (reject_.reason) {
    // 拒收按原因分流：超上限 → 413 payload_too_large；坏包 / 空包 → 400
    const UploadReject rej = reject_;
    reject_ = {nullptr, 0, nullptr};
    releasePlain();
    uploadBody_ = "";
    JsonDocument e;
    e["error"] = rej.reason;
    if (rej.code) {
      e["code"] = rej.code;
    }
    String s;
    serializeJson(e, s);
    server.sendHeader("Cache-Control", "no-store");
    server.send(rej.http ? rej.http : 400, "application/json", s);
    return false;
  }
  if (!workReady_) {
    // 只有 multipart 文件字段才走 upload 回调；直接发 JSON body 会在 WebServer 里被整包
    // 复制进 _currentArgs（正是要堵的坑），所以这种请求一律拒掉
    server.sendHeader("Cache-Control", "no-store");
    server.send(400, "application/json", R"({"error":"multipart payload required"})");
    return false;
  }
  DeserializationError err = deserializeJson(in, s_work, workLen_);
  if (err) {
    releasePlain();
    server.sendHeader("Cache-Control", "no-store");
    server.send(400, "application/json", R"({"error":"bad json"})");
    return false;
  }
  return true;
}

// 供业务侧"本轮结束、工作区可复用"时调用（摘要路径复制完文本立刻释放）
void PayloadReceiver::release() {
  releasePlain();
}
