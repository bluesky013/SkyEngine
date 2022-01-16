//
// Created by Zach Lee on 2021/12/6.
//

#include <iostream>
#include <shader/ShaderLoader.h>

int main()
{
    sky::ShaderLoader loader;
    loader.Load("BaseColor.prog");

    return 0;
}