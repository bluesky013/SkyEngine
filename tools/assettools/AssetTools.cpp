//
// Created by Zach Lee on 2021/12/6.
//

#include <iostream>
#include <model/ModelLoader.h>
#include <ProjectRoot.h>


using namespace sky;
int main()
{
    ModelLoader loader;
    loader.Load(PROJECT_ROOT + "/glTF-Sample-Models-master/2.0/DamagedHelmet/glTF/DamagedHelmet.gltf");
    loader.Save("DamagedHelmet.asset");

    return 0;
}