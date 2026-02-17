# SkyEngine

## Build

### Build Third-party deps
```cmd
python3 python/third_party.py -i <intermediate_path> -o <output_path> -e <engine_path> -p [platform]
```

* Third-party deps list:  cmake/thirdparty.json
* Third-party build args.

| args               | Description               |
|--------------------|---------------------------|
| -i, --intermediate | Third-party download path |
| -o, --output       | Binary-Output path        |
| -e, --engine       | Engine path               |
| -p, --platform     | Target Platform           |
| -c, --clear        | Clear build               |
| -t, --target       | Build Single Library      |

* Supported Platforms

| platform  | arch              |
|-----------|-------------------|
| Win32     | Window x86_64     |
| MacOS-x86 | MacOS with x86_64 |
| MacOS-arm | MacOS with arm    |
| IOS       | IOS arm64         |
| Android   | Android arm64-v8a |

### Build Engine Editor
```shell
cmake -S . -B build -G "Visual Studio 17 2022" -D3RD_PATH=${path_to_3rd}
cmake --build build
```

## Compile Shader
```shell
python .\assets\shaders\compileshaders.py
```

## Run Sample
```shell
Launcher.exe --module sample_module_name
```

![image](https://user-images.githubusercontent.com/35895395/195400282-ca50e99a-090b-4c52-a84c-d7e31a489e2f.png)

## Project Asset
```shell
AssetTool -e D:\Code\Engine\SkyEngine -p D:\Code\Engine\SkyEngine\sample\RDSceneProject
```
