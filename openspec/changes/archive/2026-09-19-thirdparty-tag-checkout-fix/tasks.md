## 1. 修 third_party.py 版本钉住

- [x] 1.1 浅克隆已有 checkout 增加 `elif tag:` 分支：fetch tag + `checkout --detach FETCH_HEAD`

## 2. astc 回到 5.7.0

- [x] 2.1 还原 `ImageCompressor.cpp` 的 3 参误改（恢复 5.7.0 的 4 参 `parent_context`）
- [x] 2.2 `cmake/patches/astc.patch` 基于 5.7.0 重新生成（仅 install 规则；删除 `-mcpu` hunk）
- [x] 2.3 `python third_party.py -p MacOS-x86 -t astc -f` 重建，checkout 确认为 5.7.0，安装头文件为 4 参签名

## 3. 验证与收尾

- [x] 3.1 `Aurora.Cook` 编译通过，`AuroraCookTest` 28/28
- [x] 3.2 `thirdparty.json` 归档 md5 同步（`ce27dbd1ee45`）
- [x] 3.3 归档 change，spec 同步
