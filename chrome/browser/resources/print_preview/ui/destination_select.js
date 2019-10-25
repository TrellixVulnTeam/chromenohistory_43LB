// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-destination-select',

  behaviors: [I18nBehavior, print_preview.SelectBehavior],

  properties: {
    activeUser: String,

    appKioskMode: Boolean,

    dark: Boolean,

    /** @type {!print_preview.Destination} */
    destination: Object,

    disabled: Boolean,

    noDestinations: Boolean,

    /** @type {!Array<!print_preview.RecentDestination>} */
    recentDestinationList: Array,
  },

  /** @private {!IronMetaElement} */
  meta_: /** @type {!IronMetaElement} */ (
      Polymer.Base.create('iron-meta', {type: 'iconset'})),

  focus: function() {
    this.$$('.md-select').focus();
  },

  /** Sets the select to the current value of |destination|. */
  updateDestination: function() {
    this.selectedValue = this.destination.key;
  },

  /**
   * @return {string} Unique identifier for the Save as PDF destination
   * @private
   */
  getPdfDestinationKey_: function() {
    return print_preview.createDestinationKey(
        print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
        print_preview.DestinationOrigin.LOCAL, '');
  },

  /**
   * @return {string} Unique identifier for the Save to Google Drive destination
   * @private
   */
  getGoogleDriveDestinationKey_: function() {
    return print_preview.createDestinationKey(
        print_preview.Destination.GooglePromotedId.DOCS,
        print_preview.DestinationOrigin.COOKIES, this.activeUser);
  },

  /**
   * Returns the iconset and icon for the selected printer. If printer details
   * have not yet been retrieved from the backend, attempts to return an
   * appropriate icon early based on the printer's sticky information.
   * @return {string} The iconset and icon for the current selection.
   * @private
   */
  getDestinationIcon_: function() {
    if (!this.selectedValue) {
      return '';
    }

    // If the destination matches the selected value, pull the icon from the
    // destination.
    if (this.destination && this.destination.key === this.selectedValue) {
      return this.destination.icon;
    }

    // Check for the Docs or Save as PDF ids first.
    const keyParams = this.selectedValue.split('/');
    if (keyParams[0] === print_preview.Destination.GooglePromotedId.DOCS) {
      return 'print-preview:save-to-drive';
    }
    if (keyParams[0] ===
        print_preview.Destination.GooglePromotedId.SAVE_AS_PDF) {
      return 'cr:insert-drive-file';
    }

    // Otherwise, must be in the recent list.
    const recent = this.recentDestinationList.find(d => {
      return print_preview.createRecentDestinationKey(d) === this.selectedValue;
    });
    if (recent && recent.icon) {
      return recent.icon;
    }

    // The key/recent destinations don't have information about what icon to
    // use, so just return the generic print icon for now. It will be updated
    // when the destination is set.
    return 'print-preview:print';
  },

  /**
   * @return {string} An inline svg corresponding to the icon for the current
   *     destination and the image for the dropdown arrow.
   * @private
   */
  getBackgroundImages_: function() {
    const icon = this.getDestinationIcon_();
    if (!icon) {
      return '';
    }

    let iconSetAndIcon = null;
    // <if expr="chromeos">
    if (this.noDestinations) {
      iconSetAndIcon = ['cr', 'error'];
    }
    // </if>
    iconSetAndIcon = iconSetAndIcon || icon.split(':');
    const iconset = /** @type {!IronIconsetSvgElement} */ (
        this.meta_.byKey(iconSetAndIcon[0]));
    return getSelectDropdownBackground(iconset, iconSetAndIcon[1], this);
  },

  /** @private */
  onProcessSelectChange: function(value) {
    this.fire('selected-option-change', value);
  },

  /**
   * @param {!print_preview.RecentDestination} recentDestination
   * @return {string} Key for the recent destination
   * @private
   */
  getKey_: function(recentDestination) {
    return print_preview.createRecentDestinationKey(recentDestination);
  },
});
