// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/tab_helper.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

SkBitmap CreateSquareBitmapWithColor(int size, SkColor color) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, size, size);
  bitmap.allocPixels();
  bitmap.eraseColor(color);
  return bitmap;
}

void ValidateBitmapSizeAndColor(SkBitmap bitmap, int size, SkColor color) {
  // Obtain pixel lock to access pixels.
  SkAutoLockPixels lock(bitmap);
  EXPECT_EQ(color, bitmap.getColor(0, 0));
  EXPECT_EQ(size, bitmap.width());
  EXPECT_EQ(size, bitmap.height());
}

class TabHelperTest : public testing::Test {
 protected:
  TabHelperTest() {
  }

  virtual ~TabHelperTest() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabHelperTest);
};

}  // namespace

TEST_F(TabHelperTest, ConstrainBitmapsToSizes) {
  std::set<int> desired_sizes;
  desired_sizes.insert(16);
  desired_sizes.insert(32);
  desired_sizes.insert(128);
  desired_sizes.insert(256);

  {
    std::vector<SkBitmap> bitmaps;
    bitmaps.push_back(CreateSquareBitmapWithColor(16, SK_ColorRED));
    bitmaps.push_back(CreateSquareBitmapWithColor(32, SK_ColorGREEN));
    bitmaps.push_back(CreateSquareBitmapWithColor(48, SK_ColorBLUE));
    bitmaps.push_back(CreateSquareBitmapWithColor(144, SK_ColorYELLOW));

    std::vector<SkBitmap> results(
        extensions::TabHelper::ConstrainBitmapsToSizes(bitmaps, desired_sizes));

    EXPECT_EQ(3u, results.size());
    ValidateBitmapSizeAndColor(results[0], 16, SK_ColorRED);
    ValidateBitmapSizeAndColor(results[1], 32, SK_ColorGREEN);
    ValidateBitmapSizeAndColor(results[2], 128, SK_ColorYELLOW);
  }
  {
    std::vector<SkBitmap> bitmaps;
    bitmaps.push_back(CreateSquareBitmapWithColor(512, SK_ColorRED));
    bitmaps.push_back(CreateSquareBitmapWithColor(18, SK_ColorGREEN));
    bitmaps.push_back(CreateSquareBitmapWithColor(33, SK_ColorBLUE));
    bitmaps.push_back(CreateSquareBitmapWithColor(17, SK_ColorYELLOW));

    std::vector<SkBitmap> results(
        extensions::TabHelper::ConstrainBitmapsToSizes(bitmaps, desired_sizes));

    EXPECT_EQ(3u, results.size());
    ValidateBitmapSizeAndColor(results[0], 16, SK_ColorYELLOW);
    ValidateBitmapSizeAndColor(results[1], 32, SK_ColorBLUE);
    ValidateBitmapSizeAndColor(results[2], 256, SK_ColorRED);
  }
}
