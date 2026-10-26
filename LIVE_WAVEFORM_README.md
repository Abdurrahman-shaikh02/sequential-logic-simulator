# Live waveform controls

The GUI now starts with only the initial Green state; it does not draw future states.
Each Step Clock click POSTs /api/live with enable and reset and appends the newly evaluated low/high clock samples to the waveform.
The Python backend reruns the existing C++ FSM testbench with the complete stimulus history up to the current edge so that the FSM state is reconstructed correctly; unlike a persistent C++ process it does not keep the executable running between requests. This is an intentional compatibility approach using the existing testbench engine.
Reset clears the input history and restores the initial Green state. Switch changes apply on the next Tick. Auto Clock repeats the live step.
The Advanced section still supports executing complete generic .tb testbenches, which can show their complete prerecorded waveforms.
All seven C++ testing categories are unchanged.
