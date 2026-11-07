#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>

#include "llm_limits.h"

// 请求体管线：WebServer 的 upload 回调 → gzip 解压 / 明文直收 → 静态工作区 → 零拷贝解析。
// WebServer 是串行处理的，同一时刻只有一个请求在用它；状态收在成员里，调用方持有一个实例。
// 实例应作为全局/静态对象（工作区是类静态成员，放 .bss），不要放栈上。
class PayloadReceiver {
 public:
  // 作为 server.on(uri, HTTP_POST, handler, uploadHandler) 的 uploadHandler
  void uploading(WebServer &server);

  // 取本次请求的 JSON 入参：数据已在工作区里（明文直收 / gzip 解压），就地解析。
  // 失败时这里已经把 413/400 发回去了，返回 false。
  bool read(WebServer &server, JsonDocument &in);

  // 本轮结束、工作区可以给下一个请求复用（只标记，不 free）
  void release();

 private:
  // 本轮结束：只是标记丢弃，不 free（工作区是静态的）
  void releasePlain();

  // 单请求明文上限 = 工作区大小（留 1KB 给解析时的就地改写与结尾 NUL）。
  // 工作区是静态的，所以这个上限是确定值，不再随堆碎片浮动。
  static size_t payloadCap();

  // 解析 gzip 头/尾：算出 deflate 数据起始偏移与解压后的原始长度（ISIZE）
  static bool gzipParse(const char *gz, size_t gzLen, size_t *dataOff, size_t *origLen);

  // 解压 raw deflate（ROM 里的 tinfl），结果写进调用方提供的工作区
  static bool gzipInflate(const char *gz, size_t gzLen, size_t dataOff, size_t origLen, char *out,
                          size_t outCap, size_t *outLen);

  // 工作区：静态分配、地址固定，明文 JSON 与解压结果都落在这里（零拷贝解析）。
  // 明文上传直接累积进来，gzip 上传先收到 uploadBody_（压缩数据很小）再解压进来。
  static char s_work[WORK_BUF_SIZE + 1];

  // 本次上传被拒的原因：上传回调里先记下，read 里再统一回给客户端。
  // 分类要准：超上限回 413 payload_too_large；坏包 / 空包是客户端问题，回 400。
  struct UploadReject {
    const char *reason;  // 文案；nullptr 表示没有拒收
    int http;            // 回给客户端的 HTTP 状态码
    const char *code;    // 机器可读错误码（可为空）
  };

  size_t workLen_ = 0;      // 工作区里现有多少字节
  bool workReady_ = false;  // 工作区里是不是一块完整可解析的 JSON
  String uploadBody_;       // 只在 gzip 上传时用来接压缩数据
  bool gzipUpload_ = false;
  UploadReject reject_ = {nullptr, 0, nullptr};
  size_t uploadCap_ = 0;  // 本次上传的字节预算（开始时定一次）
};
