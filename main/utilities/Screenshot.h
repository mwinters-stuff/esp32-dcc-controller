#pragma once

namespace utilities {

enum class CaptureStatus {
  Ready,      // Ready for new capture
  InProgress, // Capture currently in progress
};

bool ensureScreenshotStorageMounted();
bool saveActiveScreenScreenshot();
void requestActiveScreenScreenshot();
CaptureStatus getCaptureStatus();
bool isInputPausedForCapture();
void pauseInputDuringCapture();
void resumeInputAfterCapture();

} // namespace utilities
