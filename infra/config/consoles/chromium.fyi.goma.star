luci.console_view(
    name = 'chromium.fyi.goma',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/Win Builder Goma Canary',
            category = 'win|rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Win Builder (dbg) Goma Canary',
            category = 'win|dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/win32-archive-rel-goma-canary-localoutputcache',
            category = 'win|rel',
            short_name = 'loc',
        ),
        luci.console_view_entry(
            builder = 'ci/Win cl.exe Goma Canary LocalOutputCache',
            category = 'cl.exe|rel',
            short_name = 'loc',
        ),
        luci.console_view_entry(
            builder = 'ci/Win7 Builder Goma Canary',
            category = 'win7|rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Win7 Builder (dbg) Goma Canary',
            category = 'win7|dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/WinMSVC64 Goma Canary',
            category = 'cl.exe|rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac Builder Goma Canary',
            category = 'mac|rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac Builder (dbg) Goma Canary',
            category = 'mac|dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/mac-archive-rel-goma-canary',
            category = 'mac|rel',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac Builder (dbg) Goma Canary (clobber)',
            category = 'mac|dbg',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'ci/mac-archive-rel-goma-canary-localoutputcache',
            category = 'mac|rel',
            short_name = 'loc',
        ),
        luci.console_view_entry(
            builder = 'ci/chromeos-amd64-generic-rel-goma-canary',
            category = 'cros|rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Builder Goma Canary',
            category = 'linux|rel',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-archive-rel-goma-canary',
            category = 'linux|rel',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-archive-rel-goma-canary-localoutputcache',
            category = 'linux|rel',
            short_name = 'loc',
        ),
        luci.console_view_entry(
            builder = 'ci/android-archive-dbg-goma-canary',
            category = 'android|dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-device-goma-canary-clobber',
            category = 'ios|rel',
            short_name = 'clb',
        ),
        # RBE
        luci.console_view_entry(
            builder = 'ci/linux-archive-rel-goma-rbe-canary',
            category = 'rbe|linux|rel',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-archive-rel-goma-rbe-ats-canary',
            category = 'rbe|linux|rel',
            short_name = 'ats',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Builder Goma RBE Canary',
            category = 'rbe|linux|rel',
        ),
        luci.console_view_entry(
            builder = 'ci/chromeos-amd64-generic-rel-goma-rbe-canary',
            category = 'rbe|cros|rel',
        ),
        luci.console_view_entry(
            builder = 'ci/android-archive-dbg-goma-rbe-canary',
            category = 'rbe|android|dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/android-archive-dbg-goma-rbe-ats-canary',
            category = 'rbe|android|dbg',
            short_name = 'ats',
        ),
        luci.console_view_entry(
            builder = 'ci/mac-archive-rel-goma-rbe-canary',
            category = 'rbe|mac|rel',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac Builder (dbg) Goma RBE Canary (clobber)',
            category = 'rbe|mac|dbg',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-device-goma-rbe-canary-clobber',
            category = 'rbe|ios',
            short_name = 'clb',
        ),
    ],
)
