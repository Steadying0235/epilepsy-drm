//
// Created by ao on 6/14/24.
//

#import <vector>


#ifndef USERSPACE_DETECT_IMAGE_H
#define USERSPACE_DETECT_IMAGE_H

#endif //USERSPACE_DETECT_IMAGE_H

#include <GL/gl.h>

#pragma once


int detect_epileptic_image(std::vector<std::vector<unsigned int>> images);

int detect_epileptic_image_opengl(std::vector<GLuint> textures);