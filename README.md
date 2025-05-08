# Arduino Water Cooler

Authors: Aidan Kirk, Gisselle Cruz-Robinson, John Michael-Libed

We simulated a water cooler on an Arduino to learn about embedded systems concepts. The Arduino is equpped with a water sensor, a temperature/humidity sensor, a fan motor, a stepper motor, a real time clock module, button inputs, and LED/LCD screen outputs.

Our code uses state logic to switch between the different functional states of a water cooler. This includes the DISABLED state, indicating no logic is being formed. The IDLE state, indicating the temperature of the room does not need to be cooled. The RUNNING state, indicating that the cooler is using the fan to cool down the environment. The ERROR state, indicating that there is no water for the fan to disperse.

Our goal is to familiarize ourselves with low-level circuitry using breadboards and low-level code using C. We opted to avoid common Arduino standard library functions when coding to learn how to program systems through register values.