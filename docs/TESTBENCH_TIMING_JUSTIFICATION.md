# Testbench timing and clock justification

## Time unit

The testbench uses integer, discrete simulation-time units. `@0`, `@5`, and
`@10` are event timestamps, not wall-clock delays. The simulator advances
between the timestamps at which an input changes, an assertion is scheduled,
or a clock edge occurs. It does not sleep for five or ten real seconds.

The waveform writer currently labels VCD timestamps with `$timescale 1ns`.
Therefore one testbench time unit is represented as one nanosecond in the
exported VCD. The unit is a convention for interpreting waveform timestamps;
it does not model physical gate propagation delay. Combinational evaluation is
performed when events are processed.

## Why examples use 10-unit clock periods

`CLOCK clk 10` means a 10-unit period. With no third argument, the clock's
high duration defaults to half the period (5 units), producing a 50% duty-cycle
clock. This gives a clear low phase and high phase and makes rising edges occur
at 5, 15, 25, ... . Inputs can be changed between edges so a test can verify
that sequential elements sample data at the active edge rather than when the
input first changes.

The period of 10 is a convenient, readable testbench scale, not a hardware
requirement. A different positive period can be selected for a different test.

## Asymmetric clock support

The testbench grammar supports an optional third `CLOCK` argument:

```text
CLOCK clk <period> <high_duration>
```

For example:

```text
CLOCK clk 10 3
```

This gives a 10-unit period with 3 units HIGH and 7 units LOW. The rising edge
occurs at t=3, then t=13, t=23, ...; the falling edge occurs at t=10, 20, 30,
... . This is an asymmetric (30% duty-cycle) clock. The implementation
schedules both transitions explicitly; it does not assume the HIGH and LOW
phases have equal lengths.

If the third argument is omitted, invalid, or not strictly between zero and
the period, the implementation falls back to half-period HIGH duration.

## Why this matters

- The period controls how often the sequential circuit receives a clock cycle.
- The HIGH duration controls the duty cycle independently of the period.
- Scheduling both edges allows the waveform to show the complete clock shape.
- Inputs should be applied before the intended active edge when testing
  edge-triggered flip-flops or FSMs.
- Assertions should be placed after the relevant edge when checking the
  resulting state/output, rather than at the same timestamp if the test is
  intended to check post-edge behavior.

## Current implementation boundary

The simulator is a functional, event-driven logic simulator. The time values
order input, clock, and assertion events; they are not a transistor-level or
gate-delay model. The VCD export uses a fixed `1ns` timescale today. Changing
that label or adding physical delay modeling would be a separate feature.
