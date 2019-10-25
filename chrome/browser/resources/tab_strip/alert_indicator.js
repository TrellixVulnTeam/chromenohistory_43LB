// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from './custom_element.js';
import {TabAlertState} from './tabs_api_proxy.js';

/** @const {string} */
const MAX_WIDTH = '16px';

export class AlertIndicatorElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();

    /** @private {!TabAlertState} */
    this.alertState_;

    /** @private {number} */
    this.fadeDurationMs_ = 125;

    /**
     * An animation that is currently in-flight to fade the element in.
     * @private {?Animation}
     */
    this.fadeInAnimation_ = null;

    /**
     * An animation that is currently in-flight to fade the element out.
     * @private {?Animation}
     */
    this.fadeOutAnimation_ = null;

    /**
     * A promise that resolves when the fade out animation finishes or rejects
     * if a fade out animation is canceled.
     * @private {?Promise}
     */
    this.fadeOutAnimationPromise_ = null;
  }

  /** @return {!TabAlertState} */
  get alertState() {
    return this.alertState_;
  }

  /** @param {!TabAlertState} alertState */
  set alertState(alertState) {
    this.setAttribute('alert-state_', alertState);
    this.alertState_ = alertState;
  }

  /** @param {number} duration */
  overrideFadeDurationForTesting(duration) {
    this.fadeDurationMs_ = duration;
  }

  show() {
    if (this.fadeOutAnimation_) {
      // Cancel any fade out animations to prevent the element from fading out
      // and being removed. At this point, the tab's alertStates have changed
      // to a state in which this indicator should be visible.
      this.fadeOutAnimation_.cancel();
      this.fadeOutAnimation_ = null;
      this.fadeOutAnimationPromise_ = null;
    }

    if (this.fadeInAnimation_) {
      // If the element was already faded in, don't fade it in again
      return;
    }


    if (this.alertState_ === TabAlertState.MEDIA_RECORDING ||
        this.alertState_ === TabAlertState.TAB_CAPTURING ||
        this.alertState_ === TabAlertState.DESKTOP_CAPTURING) {
      // Fade in and out 2 times and then fade in
      const totalDuration = 2600;
      this.fadeInAnimation_ = this.animate(
          [
            {opacity: 0, maxWidth: 0, offset: 0},
            {opacity: 1, maxWidth: MAX_WIDTH, offset: 200 / totalDuration},
            {opacity: 0, maxWidth: MAX_WIDTH, offset: 1200 / totalDuration},
            {opacity: 1, maxWidth: MAX_WIDTH, offset: 1400 / totalDuration},
            {opacity: 0, maxWidth: MAX_WIDTH, offset: 2400 / totalDuration},
            {opacity: 1, maxWidth: MAX_WIDTH, offset: 1},
          ],
          {
            duration: totalDuration,
            easing: 'linear',
            fill: 'forwards',
          });
    } else {
      this.fadeInAnimation_ = this.animate(
          [
            {opacity: 0, maxWidth: 0},
            {opacity: 1, maxWidth: MAX_WIDTH},
          ],
          {
            duration: this.fadeDurationMs_,
            fill: 'forwards',
          });
    }
  }

  /** @return {!Promise} */
  hide() {
    if (this.fadeInAnimation_) {
      // Cancel any fade in animations to prevent the element from fading in. At
      // this point, the tab's alertStates have changed to a state in which this
      // indicator should not be visible.
      this.fadeInAnimation_.cancel();
      this.fadeInAnimation_ = null;
    }

    if (this.fadeOutAnimationPromise_) {
      return this.fadeOutAnimationPromise_;
    }

    this.fadeOutAnimationPromise_ = new Promise((resolve, reject) => {
      this.fadeOutAnimation_ = this.animate(
          [
            {opacity: 1, maxWidth: MAX_WIDTH},
            {opacity: 0, maxWidth: 0},
          ],
          {
            duration: this.fadeDurationMs_,
            fill: 'forwards',
          });
      this.fadeOutAnimation_.addEventListener('cancel', () => {
        reject();
      });
      this.fadeOutAnimation_.addEventListener('finish', () => {
        this.remove();
        this.fadeOutAnimation_ = null;
        this.fadeOutAnimationPromise_ = null;
        resolve();
      });
    });

    return this.fadeOutAnimationPromise_;
  }
}

customElements.define('tabstrip-alert-indicator', AlertIndicatorElement);
