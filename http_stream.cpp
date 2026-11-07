#include "http_stream.h"

Endpoint parseUrl(const String& url) {
  Endpoint ep;
  ep.port = url.startsWith("https://") ? 443 : 80;
  String rest = url;
  if (rest.startsWith("https://")) {
    rest = rest.substring(8);
    ep.port = 443;
  } else if (rest.startsWith("http://")) {
    rest = rest.substring(7);
    ep.port = 80;
  }
  int slash = rest.indexOf('/');
  String hostPort = slash >= 0 ? rest.substring(0, slash) : rest;
  ep.path = slash >= 0 ? rest.substring(slash) : "/";
  int colon = hostPort.indexOf(':');
  if (colon >= 0) {
    ep.port = hostPort.substring(colon + 1).toInt();
    ep.host = hostPort.substring(0, colon);
  } else {
    ep.host = hostPort;
  }
  return ep;
}
