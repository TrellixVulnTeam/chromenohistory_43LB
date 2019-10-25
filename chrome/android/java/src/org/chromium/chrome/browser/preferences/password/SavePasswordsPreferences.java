// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.FragmentManager;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceCategory;
import android.support.v7.preference.PreferenceFragmentCompat;
import android.support.v7.preference.PreferenceGroup;
import android.support.v7.widget.Toolbar;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.preferences.ChromeBaseCheckBoxPreference;
import org.chromium.chrome.browser.preferences.ChromeBasePreference;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.SearchUtils;
import org.chromium.chrome.browser.preferences.TextMessagePreference;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.text.SpanApplier;

import java.util.Locale;

/**
 * The "Save passwords" screen in Settings, which allows the user to enable or disable password
 * saving, to view saved passwords (just the username and URL), and to delete saved passwords.
 */
public class SavePasswordsPreferences
        extends PreferenceFragmentCompat implements PasswordManagerHandler.PasswordListObserver,
                                                    Preference.OnPreferenceClickListener {
    public static final String PASSWORD_LEAK_DETECTION_FEATURE = "PasswordLeakDetection";

    // Keys for name/password dictionaries.
    public static final String PASSWORD_LIST_URL = "url";
    public static final String PASSWORD_LIST_NAME = "name";
    public static final String PASSWORD_LIST_PASSWORD = "password";

    // Used to pass the password id into a new activity.
    public static final String PASSWORD_LIST_ID = "id";

    // The key for saving |mSearchQuery| to instance bundle.
    private static final String SAVED_STATE_SEARCH_QUERY = "saved-state-search-query";

    public static final String PREF_SAVE_PASSWORDS_SWITCH = "save_passwords_switch";
    public static final String PREF_AUTOSIGNIN_SWITCH = "autosignin_switch";
    public static final String PREF_LEAK_DETECTION_SWITCH = "leak_detection_switch";
    public static final String PREF_KEY_MANAGE_ACCOUNT_LINK = "manage_account_link";

    // A PasswordEntryViewer receives a boolean value with this key. If set true, the the entry was
    // part of a search result.
    public static final String EXTRA_FOUND_VIA_SEARCH = "found_via_search_args";

    private static final String PREF_KEY_CATEGORY_SAVED_PASSWORDS = "saved_passwords";
    private static final String PREF_KEY_CATEGORY_EXCEPTIONS = "exceptions";
    private static final String PREF_KEY_SAVED_PASSWORDS_NO_TEXT = "saved_passwords_no_text";

    private static final int ORDER_SWITCH = 0;
    private static final int ORDER_AUTO_LEAK_DETECTION_SWITCH = 1;
    private static final int ORDER_AUTO_SIGNIN_CHECKBOX = 2;
    private static final int ORDER_MANAGE_ACCOUNT_LINK = 3;
    private static final int ORDER_SAVED_PASSWORDS = 4;
    private static final int ORDER_EXCEPTIONS = 5;
    private static final int ORDER_SAVED_PASSWORDS_NO_TEXT = 6;

    private boolean mNoPasswords;
    private boolean mNoPasswordExceptions;

    private MenuItem mHelpItem;
    private MenuItem mSearchItem;

    private String mSearchQuery;
    private Preference mLinkPref;
    private ChromeSwitchPreference mSavePasswordsSwitch;
    private ChromeBaseCheckBoxPreference mAutoSignInSwitch;
    private ChromeSwitchPreference mAutoLeakDetectionSwitch;
    private TextMessagePreference mEmptyView;
    private boolean mSearchRecorded;
    private Menu mMenu;

    /**
     * For controlling the UX flow of exporting passwords.
     */
    private ExportFlow mExportFlow = new ExportFlow();

    public ExportFlow getExportFlowForTesting() {
        return mExportFlow;
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mExportFlow.onCreate(savedInstanceState, new ExportFlow.Delegate() {
            @Override
            public Activity getActivity() {
                return SavePasswordsPreferences.this.getActivity();
            }

            @Override
            public FragmentManager getFragmentManager() {
                return SavePasswordsPreferences.this.getFragmentManager();
            }

            @Override
            public int getViewId() {
                return getView().getId();
            }
        });
        getActivity().setTitle(R.string.prefs_saved_passwords_title);
        setPreferenceScreen(getPreferenceManager().createPreferenceScreen(getStyledContext()));
        PasswordManagerHandlerProvider.getInstance().addObserver(this);

        setHasOptionsMenu(true); // Password Export might be optional but Search is always present.

        if (savedInstanceState == null) return;

        if (savedInstanceState.containsKey(SAVED_STATE_SEARCH_QUERY)) {
            mSearchQuery = savedInstanceState.getString(SAVED_STATE_SEARCH_QUERY);
            mSearchRecorded = mSearchQuery != null; // We record a search when a query is set.
        }
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        // Disable animations of preference changes.
        getListView().setItemAnimator(null);
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        mMenu = menu;
        inflater.inflate(R.menu.save_password_preferences_action_bar_menu, menu);
        menu.findItem(R.id.export_passwords).setVisible(ExportFlow.providesPasswordExport());
        menu.findItem(R.id.export_passwords).setEnabled(false);
        mSearchItem = menu.findItem(R.id.menu_id_search);
        mSearchItem.setVisible(true);
        mHelpItem = menu.findItem(R.id.menu_id_targeted_help);
        SearchUtils.initializeSearchView(
                mSearchItem, mSearchQuery, getActivity(), this::filterPasswords);
    }

    @Override
    public void onPrepareOptionsMenu(Menu menu) {
        menu.findItem(R.id.export_passwords).setEnabled(!mNoPasswords && !mExportFlow.isActive());
        super.onPrepareOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == R.id.export_passwords) {
            mExportFlow.startExporting();
            return true;
        }
        if (SearchUtils.handleSearchNavigation(item, mSearchItem, mSearchQuery, getActivity())) {
            filterPasswords(null);
            return true;
        }
        if (id == R.id.menu_id_targeted_help) {
            HelpAndFeedback.getInstance().show(getActivity(),
                    getString(R.string.help_context_passwords), Profile.getLastUsedProfile(), null);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void filterPasswords(String query) {
        mSearchQuery = query;
        mHelpItem.setShowAsAction(mSearchQuery == null ? MenuItem.SHOW_AS_ACTION_IF_ROOM
                                                       : MenuItem.SHOW_AS_ACTION_NEVER);
        rebuildPasswordLists();
    }

    /**
     * Empty screen message when no passwords or exceptions are stored.
     */
    private void displayEmptyScreenMessage() {
        mEmptyView = new TextMessagePreference(getStyledContext(), null);
        mEmptyView.setSummary(R.string.saved_passwords_none_text);
        mEmptyView.setKey(PREF_KEY_SAVED_PASSWORDS_NO_TEXT);
        mEmptyView.setOrder(ORDER_SAVED_PASSWORDS_NO_TEXT);
        mEmptyView.setDividerAllowedAbove(false);
        mEmptyView.setDividerAllowedBelow(false);
        getPreferenceScreen().addPreference(mEmptyView);
    }

    @Override
    public void onDetach() {
        super.onDetach();
        ReauthenticationManager.resetLastReauth();
    }

    void rebuildPasswordLists() {
        mNoPasswords = false;
        mNoPasswordExceptions = false;
        getPreferenceScreen().removeAll();
        if (mSearchQuery == null) {
            createSavePasswordsSwitch();
            createAutoLeakDetectionSwitch();
            createAutoSignInCheckbox();
        }
        PasswordManagerHandlerProvider.getInstance()
                .getPasswordManagerHandler()
                .updatePasswordLists();
    }

    /**
     * Removes the UI displaying the list of saved passwords or exceptions.
     * @param preferenceCategoryKey The key string identifying the PreferenceCategory to be removed.
     */
    private void resetList(String preferenceCategoryKey) {
        PreferenceCategory profileCategory =
                (PreferenceCategory) getPreferenceScreen().findPreference(preferenceCategoryKey);
        if (profileCategory != null) {
            profileCategory.removeAll();
            getPreferenceScreen().removePreference(profileCategory);
        }
    }

    /**
     * Removes the message informing the user that there are no saved entries to display.
     */
    private void resetNoEntriesTextMessage() {
        Preference message = getPreferenceScreen().findPreference(PREF_KEY_SAVED_PASSWORDS_NO_TEXT);
        if (message != null) {
            getPreferenceScreen().removePreference(message);
        }
    }

    @Override
    public void passwordListAvailable(int count) {
        resetList(PREF_KEY_CATEGORY_SAVED_PASSWORDS);
        resetNoEntriesTextMessage();

        mNoPasswords = count == 0;
        if (mNoPasswords) {
            if (mNoPasswordExceptions) displayEmptyScreenMessage();
            return;
        }

        displayManageAccountLink();

        PreferenceGroup passwordParent;
        if (mSearchQuery == null) {
            PreferenceCategory profileCategory = new PreferenceCategory(getStyledContext());
            profileCategory.setKey(PREF_KEY_CATEGORY_SAVED_PASSWORDS);
            profileCategory.setTitle(R.string.prefs_saved_passwords_title);
            profileCategory.setOrder(ORDER_SAVED_PASSWORDS);
            getPreferenceScreen().addPreference(profileCategory);
            passwordParent = profileCategory;
        } else {
            passwordParent = getPreferenceScreen();
        }
        for (int i = 0; i < count; i++) {
            SavedPasswordEntry saved = PasswordManagerHandlerProvider.getInstance()
                                               .getPasswordManagerHandler()
                                               .getSavedPasswordEntry(i);
            String url = saved.getUrl();
            String name = saved.getUserName();
            String password = saved.getPassword();
            if (shouldBeFiltered(url, name)) {
                continue; // The current password won't show with the active filter, try the next.
            }
            Preference preference = new Preference(getStyledContext());
            preference.setTitle(url);
            preference.setOnPreferenceClickListener(this);
            preference.setSummary(name);
            Bundle args = preference.getExtras();
            args.putString(PASSWORD_LIST_NAME, name);
            args.putString(PASSWORD_LIST_URL, url);
            args.putString(PASSWORD_LIST_PASSWORD, password);
            args.putInt(PASSWORD_LIST_ID, i);
            passwordParent.addPreference(preference);
        }
        mNoPasswords = passwordParent.getPreferenceCount() == 0;
        if (mNoPasswords) {
            if (count == 0) displayEmptyScreenMessage(); // Show if the list was already empty.
            if (mSearchQuery == null) {
                // If not searching, the category needs to be removed again.
                getPreferenceScreen().removePreference(passwordParent);
            } else {
                getView().announceForAccessibility(
                        getString(R.string.accessible_find_in_page_no_results));
            }
        }
    }

    /**
     * Returns true if there is a search query that requires the exclusion of an entry based on
     * the passed url or name.
     * @param url the visible URL of the entry to check. May be empty but must not be null.
     * @param name the visible user name of the entry to check. May be empty but must not be null.
     * @return Returns whether the entry with the passed url and name should be filtered.
     */
    private boolean shouldBeFiltered(final String url, final String name) {
        if (mSearchQuery == null) {
            return false;
        }
        return !url.toLowerCase(Locale.ENGLISH).contains(mSearchQuery.toLowerCase(Locale.ENGLISH))
                && !name.toLowerCase(Locale.getDefault())
                            .contains(mSearchQuery.toLowerCase(Locale.getDefault()));
    }

    @Override
    public void passwordExceptionListAvailable(int count) {
        if (mSearchQuery != null) return; // Don't show exceptions if a search is ongoing.
        resetList(PREF_KEY_CATEGORY_EXCEPTIONS);
        resetNoEntriesTextMessage();

        mNoPasswordExceptions = count == 0;
        if (mNoPasswordExceptions) {
            if (mNoPasswords) displayEmptyScreenMessage();
            return;
        }

        displayManageAccountLink();

        PreferenceCategory profileCategory = new PreferenceCategory(getStyledContext());
        profileCategory.setKey(PREF_KEY_CATEGORY_EXCEPTIONS);
        profileCategory.setTitle(R.string.section_saved_passwords_exceptions);
        profileCategory.setOrder(ORDER_EXCEPTIONS);
        getPreferenceScreen().addPreference(profileCategory);
        for (int i = 0; i < count; i++) {
            String exception = PasswordManagerHandlerProvider.getInstance()
                                       .getPasswordManagerHandler()
                                       .getSavedPasswordException(i);
            Preference preference = new Preference(getStyledContext());
            preference.setTitle(exception);
            preference.setOnPreferenceClickListener(this);
            Bundle args = preference.getExtras();
            args.putString(PASSWORD_LIST_URL, exception);
            args.putInt(PASSWORD_LIST_ID, i);
            profileCategory.addPreference(preference);
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        mExportFlow.onResume();
        rebuildPasswordLists();
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        mExportFlow.onSaveInstanceState(outState);
        if (mSearchQuery != null) {
            outState.putString(SAVED_STATE_SEARCH_QUERY, mSearchQuery);
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        PasswordManagerHandlerProvider.getInstance().removeObserver(this);
    }

    /**
     *  Preference was clicked. Either navigate to manage account site or launch the PasswordEditor
     *  depending on which preference it was.
     */
    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (preference == mLinkPref) {
            Intent intent = new Intent(
                    Intent.ACTION_VIEW, Uri.parse(PasswordUIView.getAccountDashboardURL()));
            intent.setPackage(getActivity().getPackageName());
            getActivity().startActivity(intent);
        } else {
            // Launch preference activity with PasswordEntryViewer fragment with
            // intent extras specifying the object.
            Bundle fragmentAgs = new Bundle(preference.getExtras());
            fragmentAgs.putBoolean(
                    SavePasswordsPreferences.EXTRA_FOUND_VIA_SEARCH, mSearchQuery != null);
            PreferencesLauncher.launchSettingsPage(
                    getActivity(), PasswordEntryViewer.class, fragmentAgs);
        }
        return true;
    }

    private void createSavePasswordsSwitch() {
        mSavePasswordsSwitch = new ChromeSwitchPreference(getStyledContext(), null);
        mSavePasswordsSwitch.setKey(PREF_SAVE_PASSWORDS_SWITCH);
        mSavePasswordsSwitch.setTitle(R.string.prefs_saved_passwords);
        mSavePasswordsSwitch.setOrder(ORDER_SWITCH);
        mSavePasswordsSwitch.setSummaryOn(R.string.text_on);
        mSavePasswordsSwitch.setSummaryOff(R.string.text_off);
        mSavePasswordsSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            PrefServiceBridge.getInstance().setRememberPasswordsEnabled((boolean) newValue);
            return true;
        });
        mSavePasswordsSwitch.setManagedPreferenceDelegate(
                preference -> PrefServiceBridge.getInstance().isRememberPasswordsManaged());

        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            getPreferenceScreen().addPreference(mSavePasswordsSwitch);
        }

        // Note: setting the switch state before the preference is added to the screen results in
        // some odd behavior where the switch state doesn't always match the internal enabled state
        // (e.g. the switch will say "On" when save passwords is really turned off), so
        // .setChecked() should be called after .addPreference()
        mSavePasswordsSwitch.setChecked(
                PrefServiceBridge.getInstance().isRememberPasswordsEnabled());
    }

    private void createAutoSignInCheckbox() {
        mAutoSignInSwitch = new ChromeBaseCheckBoxPreference(getStyledContext(), null);
        mAutoSignInSwitch.setKey(PREF_AUTOSIGNIN_SWITCH);
        mAutoSignInSwitch.setTitle(R.string.passwords_auto_signin_title);
        mAutoSignInSwitch.setOrder(ORDER_AUTO_SIGNIN_CHECKBOX);
        mAutoSignInSwitch.setSummary(R.string.passwords_auto_signin_description);
        mAutoSignInSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            PrefServiceBridge.getInstance().setPasswordManagerAutoSigninEnabled((boolean) newValue);
            return true;
        });
        mAutoSignInSwitch.setManagedPreferenceDelegate(
                preference -> PrefServiceBridge.getInstance().isPasswordManagerAutoSigninManaged());
        getPreferenceScreen().addPreference(mAutoSignInSwitch);
        mAutoSignInSwitch.setChecked(
                PrefServiceBridge.getInstance().isPasswordManagerAutoSigninEnabled());
    }

    private void createAutoLeakDetectionSwitch() {
        if (!ChromeFeatureList.isEnabled(PASSWORD_LEAK_DETECTION_FEATURE)) return;

        mAutoLeakDetectionSwitch = new ChromeSwitchPreference(getStyledContext(), null);
        mAutoLeakDetectionSwitch.setKey(PREF_LEAK_DETECTION_SWITCH);
        mAutoLeakDetectionSwitch.setTitle(R.string.passwords_leak_detection_switch_title);
        mAutoLeakDetectionSwitch.setOrder(ORDER_AUTO_LEAK_DETECTION_SWITCH);
        mAutoLeakDetectionSwitch.setManagedPreferenceDelegate(
                preference -> PrefServiceBridge.getInstance().isPasswordLeakDetectionManaged());

        getPreferenceScreen().addPreference(mAutoLeakDetectionSwitch);

        if (PasswordUIView.hasAccountForLeakCheckRequest()) {
            mAutoLeakDetectionSwitch.setChecked(
                    PrefServiceBridge.getInstance().isPasswordLeakDetectionEnabled());
            mAutoLeakDetectionSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
                PrefServiceBridge.getInstance().setPasswordLeakDetectionEnabled((boolean) newValue);
                return true;
            });
            mAutoLeakDetectionSwitch.setSummary(
                    R.string.passwords_leak_detection_switch_signed_in_description);
        } else {
            mAutoLeakDetectionSwitch.setChecked(false);
            mAutoLeakDetectionSwitch.setEnabled(false);
            mAutoLeakDetectionSwitch.setOnPreferenceClickListener(null);
            if (PrefServiceBridge.getInstance().isPasswordLeakDetectionEnabled()) {
                mAutoLeakDetectionSwitch.setSummary(
                        R.string.passwords_leak_detection_switch_signed_out_full_description);
            } else {
                mAutoLeakDetectionSwitch.setSummary(
                        R.string.passwords_leak_detection_switch_signed_out_partial_description);
            }
        }
    }

    private void displayManageAccountLink() {
        if (!PreferencesLauncher.isSyncingPasswordsWithoutCustomPassphrase()) {
            return;
        }
        if (mSearchQuery != null && !mNoPasswords) {
            return; // Don't add the Manage Account link if there is a search going on.
        }
        if (getPreferenceScreen().findPreference(PREF_KEY_MANAGE_ACCOUNT_LINK) != null) {
            return; // Don't add the Manage Account link if it's present.
        }
        if (mLinkPref != null) {
            // If we created the link before, reuse it.
            getPreferenceScreen().addPreference(mLinkPref);
            return;
        }
        ForegroundColorSpan colorSpan = new ForegroundColorSpan(
                ApiCompatibilityUtils.getColor(getResources(), R.color.default_text_color_link));
        SpannableString title = SpanApplier.applySpans(getString(R.string.manage_passwords_text),
                new SpanApplier.SpanInfo("<link>", "</link>", colorSpan));
        mLinkPref = new ChromeBasePreference(getStyledContext());
        mLinkPref.setKey(PREF_KEY_MANAGE_ACCOUNT_LINK);
        mLinkPref.setTitle(title);
        mLinkPref.setOnPreferenceClickListener(this);
        mLinkPref.setOrder(ORDER_MANAGE_ACCOUNT_LINK);
        getPreferenceScreen().addPreference(mLinkPref);
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    @VisibleForTesting
    Menu getMenuForTesting() {
        return mMenu;
    }

    @VisibleForTesting
    Toolbar getToolbarForTesting() {
        return getActivity().findViewById(R.id.action_bar);
    }
}
