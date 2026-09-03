#pragma once

#include <string>
#include <map>
#include <set>
#include <functional>

namespace ota {

enum class TransactionState {
    IDLE,
    CHECKING,
    UPDATE_AVAILABLE,
    DOWNLOADING,
    DOWNLOADED,
    VERIFYING,
    VERIFIED,
    INSTALLING,
    INSTALLED,
    FAILED
};

std::string transaction_state_to_string(TransactionState state);

TransactionState string_to_transaction_state(const std::string& str);

using StateTransitionCallback = std::function<void(TransactionState from, TransactionState to, const std::string& transaction_id)>;

class TransactionStateMachine {
public:
    TransactionStateMachine();

    bool can_transition(TransactionState from, TransactionState to);

    bool transition_to(TransactionState new_state, const std::string& transaction_id = "");

    TransactionState get_current_state() const;

    void set_state(TransactionState state, const std::string& transaction_id = "");

    void reset();

    void set_transition_callback(StateTransitionCallback callback);

    bool is_active() const;

    bool is_terminal() const;

    bool is_failure() const;

private:
    void log_transition(TransactionState from, TransactionState to, const std::string& transaction_id);

    TransactionState current_state_;
    StateTransitionCallback transition_callback_;
    static const std::map<TransactionState, std::set<TransactionState>> valid_transitions_;
};

}
