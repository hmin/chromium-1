// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/lib/remote_ptr.h"
#include "mojo/public/environment/environment.h"
#include "mojo/public/utility/run_loop.h"
#include "mojom/math_calculator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {

class MathCalculatorImpl : public math::Calculator {
 public:
  virtual ~MathCalculatorImpl() {}

  explicit MathCalculatorImpl(ScopedMessagePipeHandle pipe)
      : ui_(pipe.Pass(), this),
        total_(0.0) {
  }

  virtual void Clear() MOJO_OVERRIDE {
    ui_->Output(total_);
  }

  virtual void Add(double value) MOJO_OVERRIDE {
    total_ += value;
    ui_->Output(total_);
  }

  virtual void Multiply(double value) MOJO_OVERRIDE {
    total_ *= value;
    ui_->Output(total_);
  }

 private:
  RemotePtr<math::CalculatorUI> ui_;
  double total_;
};

class MathCalculatorUIImpl : public math::CalculatorUI {
 public:
  explicit MathCalculatorUIImpl(ScopedMessagePipeHandle pipe)
      : calculator_(pipe.Pass(), this),
        output_(0.0) {
  }

  bool encountered_error() const {
    return calculator_.encountered_error();
  }

  void Add(double value) {
    calculator_->Add(value);
  }

  void Subtract(double value) {
    calculator_->Add(-value);
  }

  void Multiply(double value) {
    calculator_->Multiply(value);
  }

  void Divide(double value) {
    calculator_->Multiply(1.0 / value);
  }

  double GetOutput() const {
    return output_;
  }

 private:
  // math::CalculatorUI implementation:
  virtual void Output(double value) MOJO_OVERRIDE {
    output_ = value;
  }

  RemotePtr<math::Calculator> calculator_;
  double output_;
};

class RemotePtrTest : public testing::Test {
 public:
  RemotePtrTest() {
    CreateMessagePipe(&pipe0_, &pipe1_);
  }

  virtual ~RemotePtrTest() {
  }

  void PumpMessages() {
    loop_.RunUntilIdle();
  }

 protected:
  ScopedMessagePipeHandle pipe0_;
  ScopedMessagePipeHandle pipe1_;

 private:
  Environment env_;
  RunLoop loop_;
};

TEST_F(RemotePtrTest, EndToEnd) {
  // Suppose this is instantiated in a process that has pipe0_.
  MathCalculatorImpl calculator(pipe0_.Pass());

  // Suppose this is instantiated in a process that has pipe1_.
  MathCalculatorUIImpl calculator_ui(pipe1_.Pass());

  calculator_ui.Add(2.0);
  calculator_ui.Multiply(5.0);

  PumpMessages();

  EXPECT_EQ(10.0, calculator_ui.GetOutput());
}

TEST_F(RemotePtrTest, Movable) {
  RemotePtr<math::Calculator> a;
  RemotePtr<math::Calculator> b(pipe0_.Pass(), NULL);

  EXPECT_TRUE(a.is_null());
  EXPECT_FALSE(b.is_null());

  a = b.Pass();

  EXPECT_FALSE(a.is_null());
  EXPECT_TRUE(b.is_null());
}

TEST_F(RemotePtrTest, Resettable) {
  RemotePtr<math::Calculator> a;

  EXPECT_TRUE(a.is_null());

  MessagePipeHandle handle = pipe0_.get();

  a.reset(pipe0_.Pass(), NULL);

  EXPECT_FALSE(a.is_null());

  a.reset();

  EXPECT_TRUE(a.is_null());

  // Test that handle was closed.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, CloseRaw(handle));
}

TEST_F(RemotePtrTest, EncounteredError) {
  MathCalculatorImpl* calculator = new MathCalculatorImpl(pipe0_.Pass());

  MathCalculatorUIImpl calculator_ui(pipe1_.Pass());

  calculator_ui.Add(2.0);
  PumpMessages();
  EXPECT_EQ(2.0, calculator_ui.GetOutput());
  EXPECT_FALSE(calculator_ui.encountered_error());

  calculator_ui.Multiply(5.0);
  EXPECT_FALSE(calculator_ui.encountered_error());

  // Close the other side of the pipe.
  delete calculator;

  // The state change isn't picked up locally yet.
  EXPECT_FALSE(calculator_ui.encountered_error());

  PumpMessages();

  // OK, now we see the error.
  EXPECT_TRUE(calculator_ui.encountered_error());
}

}  // namespace test
}  // namespace mojo
