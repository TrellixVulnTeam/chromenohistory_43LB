// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_INI_PARSER_H_
#define CHROME_COMMON_INI_PARSER_H_

#include <string>

#include "base/macros.h"
#include "base/values.h"

// Parses INI files in a string. Users should in inherit from this class.
// This is a very basic INI parser with these characteristics:
//  - Ignores blank lines.
//  - Ignores comment lines beginning with '#' or ';'.
//  - Duplicate key names in the same section will simply cause repeated calls
//    to HandleTriplet with the same |section| and |key| parameters.
//  - No escape characters supported.
//  - Global properties result in calls to HandleTriplet with an empty string in
//    the |section| argument.
//  - Section headers begin with a '[' character. It is recommended, but
//    not required to close the header bracket with a ']' character. All
//    characters after a closing ']' character is ignored.
//  - Key value pairs are indicated with an '=' character. Whitespace is not
//    ignored. Quoting is not supported. Everything before the first '='
//    is considered the |key|, and everything after is the |value|.
class INIParser {
 public:
  INIParser();
  virtual ~INIParser();

  // May only be called once per instance.
  void Parse(const std::string& content);

 private:
  virtual void HandleTriplet(const std::string& section,
                             const std::string& key,
                             const std::string& value) = 0;

  bool used_;
};

// Parsed values are stored as strings at the "section.key" path. Triplets with
// |section| or |key| parameters containing '.' are ignored.
class DictionaryValueINIParser : public INIParser {
 public:
  DictionaryValueINIParser();
  ~DictionaryValueINIParser() override;

  const base::DictionaryValue& root() const { return root_; }

 private:
  // INIParser implementation.
  void HandleTriplet(const std::string& section,
                     const std::string& key,
                     const std::string& value) override;

  base::DictionaryValue root_;

  DISALLOW_COPY_AND_ASSIGN(DictionaryValueINIParser);
};

#endif  // CHROME_COMMON_INI_PARSER_H_
