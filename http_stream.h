#pragma once

// 板载 HTTP 流式工具：URL 解析 + 阻塞字节读写 + chunked 请求体 + 响应头 / 流式读。
// 按职责拆在三个子头里，这里统一聚合，调用方只需 include 本头：
//   http_io.h        基础 IO：Endpoint / parseUrl / readByteBlocking / writeAll
//   http_chunked.h   请求体 chunked 编码：chunkedSend / chunkedEnd
//   http_response.h  响应头与流式 body：readResponseHead / StreamEnd / streamLines
#include "http_chunked.h"
#include "http_io.h"
#include "http_response.h"
