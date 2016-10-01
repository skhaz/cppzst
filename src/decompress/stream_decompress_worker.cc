#include "stream_decompress_worker.h"

namespace ZSTD_NODE {

  using Nan::HandleScope;
  using Nan::Callback;
  using Nan::Error;

  using v8::String;
  using v8::Local;
  using v8::Value;

  StreamDecompressWorker::StreamDecompressWorker(Callback *callback, StreamDecompressor* sd)
    : AsyncWorker(callback), sd(sd) {
    void *src = sd->alloc.Alloc(sd->pos);
    memcpy(src, sd->input, sd->pos);
    zInBuf = {src, sd->pos, 0};
    size_t dstSize = ZSTD_DStreamOutSize();
    void *dst = sd->alloc.Alloc(dstSize);
    zOutBuf = {dst, dstSize, 0};
  }

  StreamDecompressWorker::~StreamDecompressWorker() {
    sd->alloc.Free(const_cast<void*>(zInBuf.src));
    sd->alloc.Free(zOutBuf.dst);
  }

  void StreamDecompressWorker::Execute() {
    while (zInBuf.pos < zInBuf.size) {
      do {
        zOutBuf.pos = 0;
        ret = ZSTD_decompressStream(sd->zds, &zOutBuf, &zInBuf);
        if (ZSTD_isError(ret)) {
          SetErrorMessage(ZSTD_getErrorName(ret));
        }
        pushToPendingOutput();
      } while (ret == 1);
    }
  }

  void StreamDecompressWorker::pushToPendingOutput() {
    char *output = static_cast<char*>(sd->alloc.Alloc(zOutBuf.pos));
    if (output == NULL) {
      SetErrorMessage("ZSTD decompress failed, out of memory");
    }
    memcpy(output, zOutBuf.dst, zOutBuf.pos);
    Allocator::AllocatedBuffer* buf_info = Allocator::GetBufferInfo(output);
    buf_info->available = 0;
    sd->pending_output.push_back(output);
  }

  void StreamDecompressWorker::HandleOKCallback() {
    HandleScope scope;

    const int argc = 2;
    Local<Value> argv[argc] = {
      Nan::Null(),
      sd->PendingChunksAsArray()
    };
    callback->Call(argc, argv);

    sd->alloc.ReportMemoryToV8();
  }

  void StreamDecompressWorker::HandleErrorCallback() {
    HandleScope scope;

    const int argc = 1;
    Local<Value> argv[argc] = {
      Error(Nan::New<String>(ErrorMessage()).ToLocalChecked())
    };
    callback->Call(argc, argv);

    sd->alloc.ReportMemoryToV8();
  }

}
