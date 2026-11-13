// images.h - image shuffle from the SD card's /images folder: GIF (animated), PNG and JPG.
// Best at 64x32. Smaller images are centered; bigger JPGs are scaled down by 1/2, 1/4 or 1/8 to fit,
// anything still bigger is center-cropped. tools/make_images.py converts any picture or GIF to 64x32.
#pragma once
#include <Arduino.h>

int imagesScan();  // (re)read /images; returns how many images were found
int imageCount();
// Show the next image (shuffled) for about dwellMs; animated GIFs keep looping until then.
// wait(ms) is the main loop's pump(): a non-zero result (button press etc.) stops early and is returned.
int playNextImage(uint32_t dwellMs, int (*wait)(uint32_t));
