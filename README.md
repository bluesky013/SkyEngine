# SkyEngine

## Build
```shell
cmake -S . -B build -G "Visual Studio 17 2022" -D3RD_PATH=path_to_3rd
cmake --build build
```

## Compile Shader
```shell
python .\assets\shaders\compileshaders.py
```

## Run Sample
```shell
Launcher.exe --module module_name
```

![image](https://user-images.githubusercontent.com/35895395/195400282-ca50e99a-090b-4c52-a84c-d7e31a489e2f.png)

## Project Asset
```shell
AssetTool -e D:\Code\Engine\SkyEngine -p D:\Code\Engine\SkyEngine\sample\RDSceneProject
```
