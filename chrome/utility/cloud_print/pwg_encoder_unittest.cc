// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/sha1.h"
#include "chrome/utility/cloud_print/bitmap_image.h"
#include "chrome/utility/cloud_print/pwg_encoder.h"
#include "testing/gtest/include/gtest/gtest.h"

using printing::BitmapImage;
using printing::PwgEncoder;

namespace {

// SHA-1 of golden master for this test, plus null terminating character.
// File is in chrome/test/data/printing/test_pwg_generator.pwg.
const char kPWGFileSha1[] = {
  '\xda', '\x5c', '\xca', '\x36', '\x10', '\xb9', '\xa4', '\x16',
  '\x2f', '\x98', '\x1b', '\xa6', '\x5f', '\x43', '\x24', '\x33',
  '\x60', '\x43', '\x67', '\x34', '\0'
};

const int kRasterWidth = 612;
const int kRasterHeight = 792;
const int kRasterDPI = 72;

scoped_ptr<BitmapImage> MakeSampleBitmap() {
  scoped_ptr<BitmapImage> bitmap_image(new BitmapImage(
      kRasterWidth,
      kRasterHeight,
      BitmapImage::RGBA));

  uint32* bitmap_data = reinterpret_cast<uint32*>(
      bitmap_image->mutable_pixel_data());

  for (int i = 0; i < kRasterWidth * kRasterHeight; i++) {
    bitmap_data[i] = 0xFFFFFF;
  }


  for (int i = 0; i < kRasterWidth; i++) {
    for (int j = 200; j < 300; j++) {
      int row_start = j * kRasterWidth;
      uint32 red = (i * 255)/kRasterWidth;
      bitmap_data[row_start + i] = red;
    }
  }

  // To test run length encoding
  for (int i = 0; i < kRasterWidth; i++) {
    for (int j = 400; j < 500; j++) {
      int row_start = j * kRasterWidth;
      if ((i/40) % 2 == 0) {
        bitmap_data[row_start + i] = 255 << 8;
      } else {
        bitmap_data[row_start + i] = 255 << 16;
      }
    }
  }

  return bitmap_image.Pass();
}

TEST(PwgRasterTest, CompareWithMaster) {
  std::string output;
  PwgEncoder encoder;
  scoped_ptr<BitmapImage> image = MakeSampleBitmap();

  encoder.EncodeDocumentHeader(&output);
  encoder.EncodePage(*image, kRasterDPI, 1, &output);

  EXPECT_EQ(kPWGFileSha1, base::SHA1HashString(output));
}

}  // namespace
