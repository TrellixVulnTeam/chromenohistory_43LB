// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/prefilled_values_detector.h"

#include "base/no_destructor.h"
#include "base/strings/string_util.h"

namespace autofill {

const base::flat_set<std::string, std::less<>>& KnownUsernamePlaceholders() {
  // Explicitly create a |StringFlatSet| when constructing
  // kPrefilledUsernameValues to work around GCC bug 84849, which causes the
  // initializer list not to be properly forwarded to base::flat_set's
  // constructor.
  using StringFlatSet = base::flat_set<std::string, std::less<>>;
  static base::NoDestructor<StringFlatSet> kPrefilledUsernameValues(
      StringFlatSet({"___.___.___-__",
                     "+1",
                     "3~15个字符,中文字符7个以内",
                     "benutzername",
                     "client id",
                     "codice titolare",
                     "digite seu cpf ou e-mail",
                     "ds logon username",
                     "email",
                     "email address",
                     "email masih kosong",
                     "email/手機號碼",
                     "e-mail/username",
                     "e-mail address",
                     "enter username",
                     "enter user name",
                     "identifiant",
                     "kullanıcı adı",
                     "kunden-id",
                     "login",
                     "nick",
                     "nom d'usuari",
                     "nom utilisateur",
                     "rut",
                     "siret",
                     "this is usually your email address",
                     "tu dni",
                     "uid/用戶名/email",
                     "uporabnik...",
                     "user/codice",
                     "user id",
                     "user name",
                     "username",
                     "username or email",
                     "username or email:",
                     "username/email",
                     "usuario",
                     "your email address",
                     "ååååmmddxxxx",
                     "아이디 or @이하 모두 입력",
                     "Имя",
                     "Имя (логин)",
                     "Логин",
                     "Логин...",
                     "Логин (e-mail)",
                     "שם משתמש",
                     "כתובת דוא''ל",
                     "اسم العضو",
                     "اسم المستخدم",
                     "الاسم",
                     "نام کاربری",
                     "メールアドレス",
                     "อีเมล",
                     "用户名",
                     "用户名/email",
                     "邮箱/手机",
                     "帳號",
                     "請輸入身份證字號",
                     "请用微博帐号登录",
                     "请输入手机号或邮箱",
                     "请输入邮箱或手机号",
                     "邮箱/手机/展位号"}));
  return *kPrefilledUsernameValues;
}

bool PossiblePrefilledUsernameValue(const std::string& username_value,
                                    const std::string& possible_email_domain) {
  const auto& placeholders = KnownUsernamePlaceholders();

  std::string normalized_username_value = base::ToLowerASCII(
      base::TrimWhitespaceASCII(username_value, base::TRIM_ALL));

  if (normalized_username_value.empty())
    return true;

  // Check whether the prefilled value looks like "@example.com",
  // "@mail.example.com" or other strings matching the regex
  // "^@.*possible_email_domain$" where the string possible_email_domain is
  // replaced with the value of |possible_email_domain|.
  if (normalized_username_value[0] == '@' && !possible_email_domain.empty() &&
      base::EndsWith(normalized_username_value, possible_email_domain,
                     base::CompareCase::SENSITIVE)) {
    return true;
  }

  return placeholders.find(normalized_username_value) != placeholders.end();
}

}  // namespace autofill
