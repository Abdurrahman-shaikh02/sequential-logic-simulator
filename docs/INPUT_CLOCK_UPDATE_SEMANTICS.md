# Simultaneous input and clock updates

The simulator no longer represents flip-flop data/control input changes as queued operations.

- D, J, K, and T input changes are applied immediately by each flip-flop's `evaluate()` method through `setPort()`.
- An active clock edge still queues a `ChangeStateOperation`.
- The simulator completes combinational/topological propagation before draining the sequential queue.
- Therefore, when a data/control input and the active clock edge arrive in the same simulation time, `changeState()` samples the newest input value.
- This same-time case does not emit a hold-time violation warning.

`SetPortOperation` and `OperationType::SET_PORT` have been removed. `OperationQueue` is a FIFO queue and has no `reorder()` method.

The white-box test `FlipFlop_SimultaneousInputClock_UsesNewestInput` exercises positive- and negative-edge D, JK, and T flip-flops. The FSM integration test also checks the updated same-time sampling timeline.
