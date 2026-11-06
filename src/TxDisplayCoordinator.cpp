#include "TxDisplayCoordinator.h"

TxDisplayAction TxDisplayCoordinator::update(bool preparationReady,
                                             bool displayAvailable,
                                             bool marginValid,
                                             uint32_t remainingMillis) {
  if (!preparationReady) {
    reset();
    return TxDisplayAction::NONE;
  }
  if (!marginValid) {
    preparingAttempted_ = true;
    txOnAttempted_ = true;
    return TxDisplayAction::SKIP;
  }
  if (remainingMillis <= TX_ON_LEAD_MS) {
    preparingAttempted_ = true;
    if (txOnAttempted_) return TxDisplayAction::NONE;
    txOnAttempted_ = true;
    return displayAvailable && remainingMillis >= MINIMUM_MARGIN_MS
        ? TxDisplayAction::RENDER_TX_ON : TxDisplayAction::SKIP;
  }
  if (preparingAttempted_) return TxDisplayAction::NONE;
  preparingAttempted_ = true;
  return displayAvailable && remainingMillis >= MINIMUM_MARGIN_MS
      ? TxDisplayAction::RENDER_PREPARING : TxDisplayAction::SKIP;
}

void TxDisplayCoordinator::reset() {
  preparingAttempted_ = false;
  txOnAttempted_ = false;
}
