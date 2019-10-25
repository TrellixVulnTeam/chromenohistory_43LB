// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_AUTOFILL_PAYMENT_INSTRUMENT_H_
#define COMPONENTS_PAYMENTS_CORE_AUTOFILL_PAYMENT_INSTRUMENT_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/address_normalizer.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/payments/core/payment_instrument.h"

namespace payments {

class PaymentRequestBaseDelegate;

// Represents an Autofill/Payments credit card form of payment in Payment
// Request.
class AutofillPaymentInstrument
    : public PaymentInstrument,
      public autofill::payments::FullCardRequest::ResultDelegate {
 public:
  // |billing_profiles| is owned by the caller and should outlive this object.
  // |payment_request_delegate| must outlive this object.
  AutofillPaymentInstrument(
      const std::string& method_name,
      const autofill::CreditCard& card,
      bool matches_merchant_card_type_exactly,
      const std::vector<autofill::AutofillProfile*>& billing_profiles,
      const std::string& app_locale,
      PaymentRequestBaseDelegate* payment_request_delegate);
  ~AutofillPaymentInstrument() override;

  // PaymentInstrument:
  void InvokePaymentApp(PaymentInstrument::Delegate* delegate) override;
  bool IsCompleteForPayment() const override;
  uint32_t GetCompletenessScore() const override;
  bool CanPreselect() const override;
  bool IsExactlyMatchingMerchantRequest() const override;
  base::string16 GetMissingInfoLabel() const override;
  bool IsValidForCanMakePayment() const override;
  void RecordUse() override;
  base::string16 GetLabel() const override;
  base::string16 GetSublabel() const override;
  bool IsValidForModifier(const std::string& method,
                          bool supported_networks_specified,
                          const std::set<std::string>& supported_networks,
                          bool supported_types_specified,
                          const std::set<autofill::CreditCard::CardType>&
                              supported_types) const override;
  void IsValidForPaymentMethodIdentifier(
      const std::string& payment_method_identifier,
      bool* is_valid) const override;
  base::WeakPtr<PaymentInstrument> AsWeakPtr() override;
  bool HandlesShippingAddress() const override;
  bool HandlesPayerName() const override;
  bool HandlesPayerEmail() const override;
  bool HandlesPayerPhone() const override;

  // autofill::payments::FullCardRequest::ResultDelegate:
  void OnFullCardRequestSucceeded(
      const autofill::payments::FullCardRequest& full_card_request,
      const autofill::CreditCard& card,
      const base::string16& cvc) override;
  void OnFullCardRequestFailed() override;

  void RecordMissingFieldsForInstrument() const;

  // Sets whether the complete and valid autofill data for merchant's request is
  // available.
  void set_is_requested_autofill_data_available(bool available) {
    is_requested_autofill_data_available_ = available;
  }
  autofill::CreditCard* credit_card() { return &credit_card_; }
  const autofill::CreditCard* credit_card() const { return &credit_card_; }

  const std::string& method_name() const { return method_name_; }

 private:
  // Generates the basic card response and sends it to the delegate.
  void GenerateBasicCardResponse();

  // To be used as AddressNormalizer::NormalizationCallback.
  void OnAddressNormalized(bool success,
                           const autofill::AutofillProfile& normalized_profile);

  const std::string method_name_;

  // A copy of the card is owned by this object.
  autofill::CreditCard credit_card_;

  // Whether the card type (credit, debit, prepaid) matches the merchant's
  // requested card type exactly. If the merchant accepts all card types, then
  // this variable is always "true". If the merchant accepts only a subset of
  // the card types, then this variable is "false" for unknown card types.
  const bool matches_merchant_card_type_exactly_;

  // Not owned by this object, should outlive this.
  const std::vector<autofill::AutofillProfile*>& billing_profiles_;

  const std::string app_locale_;

  PaymentInstrument::Delegate* delegate_;
  PaymentRequestBaseDelegate* payment_request_delegate_;
  autofill::AutofillProfile billing_address_;

  base::string16 cvc_;

  bool is_waiting_for_card_unmask_;
  bool is_waiting_for_billing_address_normalization_;

  // True when complete and valid autofill data for merchant's request is
  // available, e.g., if merchant specifies `requestPayerEmail: true`, then this
  // variable is true only if the autofill data contains a valid email address.
  bool is_requested_autofill_data_available_ = false;

  base::WeakPtrFactory<AutofillPaymentInstrument> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AutofillPaymentInstrument);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_AUTOFILL_PAYMENT_INSTRUMENT_H_
