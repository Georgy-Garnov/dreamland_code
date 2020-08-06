#ifndef MATH_UTILS_H
#define MATH_UTILS_H

float linear_interpolation (float x, float x1, float x2, float y1, float y2);

/** Generate random alnum string of given length. */
std::string create_nonce(int len);

#endif
