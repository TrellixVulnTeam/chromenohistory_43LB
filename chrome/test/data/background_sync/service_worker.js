// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let messagePort = null;

self.addEventListener('message', event => {
  messagePort = event.data;
  messagePort.postMessage('ready');
});

self.addEventListener('sync', async event => {
  const onSync = async () => {
    switch (event.tag) {
      case 'tagSucceedsSync':
        messagePort.postMessage('onsync: ' + event.tag);
        return;
      case 'tagFailsSync':
        throw 'Try again';
    }
  };
  event.waitUntil(onSync());
});

