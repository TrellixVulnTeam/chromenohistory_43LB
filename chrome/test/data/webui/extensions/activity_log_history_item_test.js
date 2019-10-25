// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for activity-log-history-item. */
suite('ExtensionsActivityLogHistoryItemTest', function() {
  /**
   * Extension activityLogHistoryItem created before each test.
   * @type {extensions.ActivityLogHistoryItem}
   */
  let activityLogHistoryItem;
  let testVisible;

  /**
   * ActivityGroup data for the activityLogHistoryItem
   * @type {extensions.ActivityGroup}
   */
  let testActivityGroup;

  // Initialize an extension activity log item before each test.
  setup(function() {
    PolymerTest.clearBody();
    testActivityGroup = {
      activityIds: ['1'],
      key: 'i18n.getUILanguage',
      count: 1,
      activityType: chrome.activityLogPrivate.ExtensionActivityFilter.API_CALL,
      countsByUrl: new Map(),
      expanded: false
    };

    activityLogHistoryItem = new extensions.ActivityLogHistoryItem();
    activityLogHistoryItem.data = testActivityGroup;
    testVisible =
        extension_test_util.testVisible.bind(null, activityLogHistoryItem);

    document.body.appendChild(activityLogHistoryItem);
  });

  teardown(function() {
    activityLogHistoryItem.remove();
  });

  test('no page URLs shown when activity has no associated page', function() {
    Polymer.dom.flush();

    testVisible('#activity-item-main-row', true);
    testVisible('#page-url-list', false);
  });

  test('clicking the expand button shows the associated page URL', function() {
    const countsByUrl = new Map([['google.com', 1]]);

    testActivityGroup = {
      activityIds: ['2'],
      key: 'Storage.getItem',
      count: 3,
      activityType:
          chrome.activityLogPrivate.ExtensionActivityFilter.DOM_ACCESS,
      countsByUrl
    };
    activityLogHistoryItem.set('data', testActivityGroup);

    Polymer.dom.flush();

    testVisible('#activity-item-main-row', true);
    testVisible('#page-url-list', false);

    activityLogHistoryItem.$$('#activity-item-main-row').click();
    testVisible('#page-url-list', true);
  });

  test('count not shown when there is only 1 page URL', function() {
    const countsByUrl = new Map([['google.com', 1]]);

    testActivityGroup = {
      activityIds: ['3'],
      key: 'Storage.getItem',
      count: 3,
      activityType:
          chrome.activityLogPrivate.ExtensionActivityFilter.DOM_ACCESS,
      countsByUrl
    };

    activityLogHistoryItem.set('data', testActivityGroup);
    activityLogHistoryItem.$$('#activity-item-main-row').click();

    Polymer.dom.flush();

    testVisible('#activity-item-main-row', true);
    testVisible('#page-url-list', true);
    testVisible('.page-url-count', false);
  });

  test('count shown in descending order for multiple page URLs', function() {
    const countsByUrl =
        new Map([['google.com', 5], ['chrome://extensions', 10]]);

    testActivityGroup = {
      activityIds: ['1'],
      key: 'Storage.getItem',
      count: 15,
      activityType:
          chrome.activityLogPrivate.ExtensionActivityFilter.DOM_ACCESS,
      countsByUrl
    };
    activityLogHistoryItem.set('data', testActivityGroup);
    activityLogHistoryItem.$$('#activity-item-main-row').click();

    Polymer.dom.flush();

    testVisible('#activity-item-main-row', true);
    testVisible('#page-url-list', true);
    testVisible('.page-url-count', true);

    const pageUrls =
        activityLogHistoryItem.shadowRoot.querySelectorAll('.page-url');
    expectEquals(pageUrls.length, 2);

    // Test the order of the page URLs and activity count for the activity
    // log item here. Apparently a space is added at the end of the innerText,
    // hence the use of .includes.
    expectTrue(pageUrls[0]
                   .querySelector('.page-url-link')
                   .innerText.includes('chrome://extensions'));
    expectEquals(pageUrls[0].querySelector('.page-url-count').innerText, '10');

    expectTrue(pageUrls[1]
                   .querySelector('.page-url-link')
                   .innerText.includes('google.com'));
    expectEquals(pageUrls[1].querySelector('.page-url-count').innerText, '5');
  });
});
