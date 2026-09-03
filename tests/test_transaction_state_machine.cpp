#include <gtest/gtest.h>
#include "transaction/transaction_state_machine.h"

using namespace ota;

class TransactionStateMachineTest : public ::testing::Test {
protected:
    TransactionStateMachine sm;
};

TEST_F(TransactionStateMachineTest, InitialStateIsIdle) {
    EXPECT_EQ(sm.get_current_state(), TransactionState::IDLE);
}

TEST_F(TransactionStateMachineTest, ValidTransitionIdleToChecking) {
    EXPECT_TRUE(sm.can_transition(TransactionState::IDLE, TransactionState::CHECKING));
    EXPECT_TRUE(sm.transition_to(TransactionState::CHECKING, "test-tx"));
    EXPECT_EQ(sm.get_current_state(), TransactionState::CHECKING);
}

TEST_F(TransactionStateMachineTest, InvalidTransitionIdleToInstalling) {
    EXPECT_FALSE(sm.can_transition(TransactionState::IDLE, TransactionState::INSTALLING));
    EXPECT_FALSE(sm.transition_to(TransactionState::INSTALLING, "test-tx"));
    EXPECT_EQ(sm.get_current_state(), TransactionState::IDLE);
}

TEST_F(TransactionStateMachineTest, FullSuccessfulSequence) {
    EXPECT_TRUE(sm.transition_to(TransactionState::CHECKING, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::UPDATE_AVAILABLE, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::DOWNLOADING, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::DOWNLOADED, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::VERIFYING, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::VERIFIED, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::INSTALLING, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::INSTALLED, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::IDLE, "tx1"));

    EXPECT_EQ(sm.get_current_state(), TransactionState::IDLE);
}

TEST_F(TransactionStateMachineTest, FailedDownload) {
    EXPECT_TRUE(sm.transition_to(TransactionState::CHECKING, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::UPDATE_AVAILABLE, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::DOWNLOADING, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::FAILED, "tx1"));

    EXPECT_EQ(sm.get_current_state(), TransactionState::FAILED);
}

TEST_F(TransactionStateMachineTest, FailedVerification) {
    EXPECT_TRUE(sm.transition_to(TransactionState::CHECKING, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::UPDATE_AVAILABLE, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::DOWNLOADING, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::DOWNLOADED, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::VERIFYING, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::FAILED, "tx1"));

    EXPECT_EQ(sm.get_current_state(), TransactionState::FAILED);
}

TEST_F(TransactionStateMachineTest, FailedInstallation) {
    EXPECT_TRUE(sm.transition_to(TransactionState::CHECKING, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::UPDATE_AVAILABLE, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::DOWNLOADING, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::DOWNLOADED, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::VERIFYING, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::VERIFIED, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::INSTALLING, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::FAILED, "tx1"));

    EXPECT_EQ(sm.get_current_state(), TransactionState::FAILED);
}

TEST_F(TransactionStateMachineTest, NoUpdateAvailable) {
    EXPECT_TRUE(sm.transition_to(TransactionState::CHECKING, "tx1"));
    EXPECT_TRUE(sm.transition_to(TransactionState::IDLE, "tx1"));

    EXPECT_EQ(sm.get_current_state(), TransactionState::IDLE);
}

TEST_F(TransactionStateMachineTest, IsActive) {
    EXPECT_FALSE(sm.is_active());

    sm.transition_to(TransactionState::CHECKING, "tx1");
    EXPECT_TRUE(sm.is_active());

    sm.transition_to(TransactionState::UPDATE_AVAILABLE, "tx1");
    EXPECT_TRUE(sm.is_active());

    sm.set_state(TransactionState::INSTALLED, "tx1");
    EXPECT_FALSE(sm.is_active());
}

TEST_F(TransactionStateMachineTest, IsTerminal) {
    EXPECT_TRUE(sm.is_terminal());

    sm.transition_to(TransactionState::CHECKING, "tx1");
    EXPECT_FALSE(sm.is_terminal());

    sm.set_state(TransactionState::INSTALLED, "tx1");
    EXPECT_TRUE(sm.is_terminal());

    sm.set_state(TransactionState::IDLE, "tx1");
    EXPECT_TRUE(sm.is_terminal());

    sm.set_state(TransactionState::CHECKING, "tx2");
    sm.set_state(TransactionState::FAILED, "tx2");
    EXPECT_TRUE(sm.is_terminal());
}

TEST_F(TransactionStateMachineTest, IsFailure) {
    EXPECT_FALSE(sm.is_failure());

    sm.transition_to(TransactionState::CHECKING, "tx1");
    EXPECT_FALSE(sm.is_failure());

    sm.transition_to(TransactionState::FAILED, "tx1");
    EXPECT_TRUE(sm.is_failure());
}

TEST_F(TransactionStateMachineTest, Reset) {
    sm.transition_to(TransactionState::CHECKING, "tx1");
    sm.transition_to(TransactionState::DOWNLOADING, "tx1");
    sm.reset();

    EXPECT_EQ(sm.get_current_state(), TransactionState::IDLE);
}

TEST_F(TransactionStateMachineTest, SetStateDirectly) {
    sm.set_state(TransactionState::INSTALLING, "tx1");
    EXPECT_EQ(sm.get_current_state(), TransactionState::INSTALLING);
}

TEST_F(TransactionStateMachineTest, TransitionCallback) {
    TransactionState cb_from = TransactionState::IDLE;
    TransactionState cb_to = TransactionState::IDLE;
    std::string cb_tx_id;

    sm.set_transition_callback(
        [&cb_from, &cb_to, &cb_tx_id](TransactionState from, TransactionState to, const std::string& tx_id) {
            cb_from = from;
            cb_to = to;
            cb_tx_id = tx_id;
        });

    sm.transition_to(TransactionState::CHECKING, "my-tx");

    EXPECT_EQ(cb_from, TransactionState::IDLE);
    EXPECT_EQ(cb_to, TransactionState::CHECKING);
    EXPECT_EQ(cb_tx_id, "my-tx");
}

TEST_F(TransactionStateMachineTest, StateToString) {
    EXPECT_EQ(transaction_state_to_string(TransactionState::IDLE), "IDLE");
    EXPECT_EQ(transaction_state_to_string(TransactionState::CHECKING), "CHECKING");
    EXPECT_EQ(transaction_state_to_string(TransactionState::UPDATE_AVAILABLE), "UPDATE_AVAILABLE");
    EXPECT_EQ(transaction_state_to_string(TransactionState::DOWNLOADING), "DOWNLOADING");
    EXPECT_EQ(transaction_state_to_string(TransactionState::DOWNLOADED), "DOWNLOADED");
    EXPECT_EQ(transaction_state_to_string(TransactionState::VERIFYING), "VERIFYING");
    EXPECT_EQ(transaction_state_to_string(TransactionState::VERIFIED), "VERIFIED");
    EXPECT_EQ(transaction_state_to_string(TransactionState::INSTALLING), "INSTALLING");
    EXPECT_EQ(transaction_state_to_string(TransactionState::INSTALLED), "INSTALLED");
    EXPECT_EQ(transaction_state_to_string(TransactionState::FAILED), "FAILED");
}

TEST_F(TransactionStateMachineTest, StringToState) {
    EXPECT_EQ(string_to_transaction_state("IDLE"), TransactionState::IDLE);
    EXPECT_EQ(string_to_transaction_state("CHECKING"), TransactionState::CHECKING);
    EXPECT_EQ(string_to_transaction_state("UPDATE_AVAILABLE"), TransactionState::UPDATE_AVAILABLE);
    EXPECT_EQ(string_to_transaction_state("DOWNLOADING"), TransactionState::DOWNLOADING);
    EXPECT_EQ(string_to_transaction_state("DOWNLOADED"), TransactionState::DOWNLOADED);
    EXPECT_EQ(string_to_transaction_state("VERIFYING"), TransactionState::VERIFYING);
    EXPECT_EQ(string_to_transaction_state("VERIFIED"), TransactionState::VERIFIED);
    EXPECT_EQ(string_to_transaction_state("INSTALLING"), TransactionState::INSTALLING);
    EXPECT_EQ(string_to_transaction_state("INSTALLED"), TransactionState::INSTALLED);
    EXPECT_EQ(string_to_transaction_state("FAILED"), TransactionState::FAILED);
    EXPECT_EQ(string_to_transaction_state("UNKNOWN"), TransactionState::IDLE);
}

TEST_F(TransactionStateMachineTest, InvalidTransitionFromFailed) {
    sm.transition_to(TransactionState::CHECKING, "tx1");
    sm.transition_to(TransactionState::FAILED, "tx1");

    EXPECT_FALSE(sm.can_transition(TransactionState::FAILED, TransactionState::CHECKING));
    EXPECT_FALSE(sm.can_transition(TransactionState::FAILED, TransactionState::DOWNLOADING));
    EXPECT_TRUE(sm.can_transition(TransactionState::FAILED, TransactionState::IDLE));
}

TEST_F(TransactionStateMachineTest, InvalidTransitionFromInstalled) {
    sm.transition_to(TransactionState::CHECKING, "tx1");
    sm.transition_to(TransactionState::UPDATE_AVAILABLE, "tx1");
    sm.transition_to(TransactionState::DOWNLOADING, "tx1");
    sm.transition_to(TransactionState::DOWNLOADED, "tx1");
    sm.transition_to(TransactionState::VERIFYING, "tx1");
    sm.transition_to(TransactionState::VERIFIED, "tx1");
    sm.transition_to(TransactionState::INSTALLING, "tx1");
    sm.transition_to(TransactionState::INSTALLED, "tx1");

    EXPECT_FALSE(sm.can_transition(TransactionState::INSTALLED, TransactionState::CHECKING));
    EXPECT_FALSE(sm.can_transition(TransactionState::INSTALLED, TransactionState::DOWNLOADING));
    EXPECT_TRUE(sm.can_transition(TransactionState::INSTALLED, TransactionState::IDLE));
}
