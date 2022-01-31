//
// Created by Zach Lee on 2021/12/6.
//

#include <iostream>
#include <model/ModelLoader.h>
#include <shader/ShaderLoader.h>
#include <ProjectRoot.h>


using namespace sky;
int main()
{
//    ModelLoader modelLoader;
//    modelLoader.Load(PROJECT_ROOT + "/glTF-Sample-Models-master/2.0/DamagedHelmet/glTF/DamagedHelmet.gltf");
//    modelLoader.Save("DamagedHelmet.model");

    ShaderLoader shaderLoader;
    shaderLoader.Load(PROJECT_ROOT + "/shaders/BaseColor.prog");
    shaderLoader.Save("BaseColor.prog");

    return 0;
}