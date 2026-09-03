#include "transaction/transaction_state_machine.h"
#include "logging/logger.h"

namespace ota {

const std::map<TransactionState, std::set<TransactionState>> TransactionStateMachine::valid_transitions_ = {
    {TransactionState::IDLE, {TransactionState::CHECKING}},
    {TransactionState::CHECKING, {TransactionState::UPDATE_AVAILABLE, TransactionState::IDLE, TransactionState::FAILED}},
    {TransactionState::UPDATE_AVAILABLE, {TransactionState::DOWNLOADING, TransactionState::IDLE, TransactionState::FAILED}},
    {TransactionState::DOWNLOADING, {TransactionState::DOWNLOADED, TransactionState::FAILED}},
    {TransactionState::DOWNLOADED, {TransactionState::VERIFYING, TransactionState::FAILED}},
    {TransactionState::VERIFYING, {TransactionState::VERIFIED, TransactionState::FAILED}},
    {TransactionState::VERIFIED, {TransactionState::INSTALLING, TransactionState::FAILED}},
    {TransactionState::INSTALLING, {TransactionState::INSTALLED, TransactionState::FAILED}},
    {TransactionState::INSTALLED, {TransactionState::IDLE}},
    {TransactionState::FAILED, {TransactionState::IDLE}}
};

std::string transaction_state_to_string(TransactionState state) {
    switch (state) {
        case TransactionState::IDLE:             return "IDLE";
        case TransactionState::CHECKING:         return "CHECKING";
        case TransactionState::UPDATE_AVAILABLE: return "UPDATE_AVAILABLE";
        case TransactionState::DOWNLOADING:      return "DOWNLOADING";
        case TransactionState::DOWNLOADED:       return "DOWNLOADED";
        case TransactionState::VERIFYING:        return "VERIFYING";
        case TransactionState::VERIFIED:         return "VERIFIED";
        case TransactionState::INSTALLING:       return "INSTALLING";
        case TransactionState::INSTALLED:        return "INSTALLED";
        case TransactionState::FAILED:           return "FAILED";
        default:                                return "UNKNOWN";
    }
}

TransactionState string_to_transaction_state(const std::string& str) {
    if (str == "IDLE")             return TransactionState::IDLE;
    if (str == "CHECKING")         return TransactionState::CHECKING;
    if (str == "UPDATE_AVAILABLE") return TransactionState::UPDATE_AVAILABLE;
    if (str == "DOWNLOADING")      return TransactionState::DOWNLOADING;
    if (str == "DOWNLOADED")       return TransactionState::DOWNLOADED;
    if (str == "VERIFYING")        return TransactionState::VERIFYING;
    if (str == "VERIFIED")         return TransactionState::VERIFIED;
    if (str == "INSTALLING")       return TransactionState::INSTALLING;
    if (str == "INSTALLED")        return TransactionState::INSTALLED;
    if (str == "FAILED")           return TransactionState::FAILED;
    return TransactionState::IDLE;
}

TransactionStateMachine::TransactionStateMachine()
    : current_state_(TransactionState::IDLE) {
}

bool TransactionStateMachine::can_transition(TransactionState from, TransactionState to) {
    auto it = valid_transitions_.find(from);
    if (it == valid_transitions_.end()) {
        return false;
    }
    return it->second.find(to) != it->second.end();
}

bool TransactionStateMachine::transition_to(TransactionState new_state, const std::string& transaction_id) {
    if (!can_transition(current_state_, new_state)) {
        Logger::instance().error("transaction",
            "Invalid transition: " + transaction_state_to_string(current_state_) +
            " -> " + transaction_state_to_string(new_state));
        return false;
    }

    TransactionState old_state = current_state_;
    current_state_ = new_state;
    log_transition(old_state, new_state, transaction_id);
    return true;
}

TransactionState TransactionStateMachine::get_current_state() const {
    return current_state_;
}

void TransactionStateMachine::set_state(TransactionState state, const std::string& transaction_id) {
    TransactionState old_state = current_state_;
    current_state_ = state;
    log_transition(old_state, state, transaction_id);
}

void TransactionStateMachine::reset() {
    current_state_ = TransactionState::IDLE;
}

void TransactionStateMachine::set_transition_callback(StateTransitionCallback callback) {
    transition_callback_ = callback;
}

bool TransactionStateMachine::is_active() const {
    return current_state_ != TransactionState::IDLE &&
           current_state_ != TransactionState::INSTALLED &&
           current_state_ != TransactionState::FAILED;
}

bool TransactionStateMachine::is_terminal() const {
    return current_state_ == TransactionState::INSTALLED ||
           current_state_ == TransactionState::FAILED ||
           current_state_ == TransactionState::IDLE;
}

bool TransactionStateMachine::is_failure() const {
    return current_state_ == TransactionState::FAILED;
}

void TransactionStateMachine::log_transition(TransactionState from, TransactionState to, const std::string& transaction_id) {
    std::string msg = transaction_state_to_string(from) + " -> " + transaction_state_to_string(to);
    if (!transaction_id.empty()) {
        msg += " [transaction: " + transaction_id + "]";
    }

    if (to == TransactionState::FAILED) {
        Logger::instance().error("transaction", msg);
    } else {
        Logger::instance().info("transaction", msg);
    }

    if (transition_callback_) {
        transition_callback_(from, to, transaction_id);
    }
}

}
