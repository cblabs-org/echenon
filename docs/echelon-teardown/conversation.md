# OK - I've been doing a bunch of investigation into the electrics in the Echelon …

**Source:** <redacted-session-link>
**Date:** 19 Sep 2026
**Turns:** 14 | **Attachments:** 17 | **Canvas:** 0

---

## You — 19 Sep 2026, 19:05

OK - I've been doing a bunch of investigation into the electrics in the Echelon EX3 bike that I have an its interesting.  I want to do a brain dump and then add a bunch of photos so that we can document the bike's internals.

Before we do that - can you do a bit of research on the internet and see if anything has been done like that before?

## Gemini — 19 Sep 2026, 19:05

This sounds like an awesome project! I did some digging, and here is the current landscape of what is already out there regarding the Echelon EX3 internals and reverse engineering:

**1\. The Software & Protocol Side (Heavily Documented)** The Bluetooth Low Energy (BLE) protocol that the EX3 uses has been thoroughly reverse-engineered by the community. The most prominent example of this is the **qDomyos-Zwift (QZ) app** developed by Roberto Viola. The community has mapped out how the bike broadcasts its resistance, cadence, and power metrics so it can be bridged to third-party platforms like Zwift or the Peloton app. If you are looking at the data packets and Bluetooth payloads, that trail has largely been blazed.

**2\. The Hardware & Electrical Side (Very Sparse)** This is where your documentation will be incredibly valuable. There is surprisingly little deep-dive hardware documentation out there:

*   **Basic Disassembly:** There are a few standard YouTube tutorials showing how to take off the plastic shrouds, remove the flywheel, or extract the pedals and cranks.
*   **Motherboard Swaps:** There are a few community discussions walking through how to replace the main Bluetooth/motherboard, noting that it sits directly below the red resistance knob.
*   **Missing Deep Dives:** Detailed PCB analysis, wiring harness mapping, component identification, or explanations of how the console interfaces with the magnetic resistance motor are virtually non-existent.

**The Verdict** A dedicated "brain dump" with high-quality photos of the EX3's internal wiring, PCBs, and mechanical resistance assemblies would be a fantastic, unique contribution to the hardware hacking and DIY repair communities.

Whenever you are ready, feel free to start dropping your notes and photos here. We can organize the information, clean up the technical descriptions, and format it into a definitive teardown guide. What part of the bike do you want to start with?

---

## You — 19 Sep 2026, 19:08

There are multiple components. I took it apart and had a look inside, and really there are the components in the handlebar, which includes the circuit board and the switch, and then there are the components in the bike itself, which really have the cadence monitor, the resistance knob, and also the resistance setter. All of those pieces are distinct, and I've looked into each of those and also into the wiring looms.

## Gemini — 19 Sep 2026, 19:08

That is a clean architectural breakdown. Grouping the teardown into the Control/Communications (handlebars), Sensors/Actuators (main body), and Interconnects (wiring looms) gives us a perfect structure for the documentation.

Drop your first batch of photos and notes whenever you are ready. Which of those three sections do you want to tackle first?

---

## You — 19 Sep 2026, 19:11

ok firstly here is the powerpack.  I confirmed with my volt meter that it delivers a 9v DC to the bike motherboard

📎 Attached: image_cabb80.jpg

## Gemini — 19 Sep 2026, 19:11

The specs from `image_cabb80.jpg` align perfectly with your 9V reading. Here is the formal breakdown of the power supply for the documentation:

*   **Model:** YS25-0903000
*   **Input:** 100-240V~ 50/60Hz, 0.7A Max
*   **Output:** 9.0V DC, 3.0A, 27.0W
*   **Polarity:** Center positive

.

What component are we looking at next? We can follow the power down into the wiring looms or jump straight to one of the control boards.

---

## You — 19 Sep 2026, 19:12

Okay, I'm going to show you some pictures now of the place where the wires come out of the bike and into the motherboard, so it's underneath the handlebars of the EX3. And there are two plugs that go into through the casing into the motherboard that you can see here, which I'll discuss a bit more later. Just one thing to note, it looks like these are JST plugs. The black one is a nine-pin JST-SM plug. The other one is a JST-3, I think. It just looks like a standard one, and I'll come on to what those are shortly.

📎 Attached: image_cabecb.jpg, image_cabf0c.jpg

## Gemini — 19 Sep 2026, 19:12

These photos provide a great look at the interface between the bike's main body and the control console. Here is a breakdown of the connections entering the motherboard casing, based on the images provided:

### Handlebar Console Connections

The wiring loom from the main body of the bike enters the back of the plastic console casing behind the handlebars. :

. You noted this is likely a JST-SM connector. . Given the number of pins, this loom likely carries the 9V DC power up to the board, as well as the data signals from the cadence sensor and the resistance knob/motor assembly below. . You noted this resembles a standard JST-3 plug. .

, giving us a rough idea of this specific unit's production timeline.

What is the next step in the teardown? Do you want to open that casing to look at the motherboard itself, or trace those wires back down into the frame?

---

## You — 19 Sep 2026, 19:14

These are the plugs going into it. You can see the nine pin with all the different colored wires. If you can make a note of those colors, I'll tell you what's going to them. The small three pin actually has three wires inside, a red, a black, and an orange wire, and I'll come onto what those are later as well.

📎 Attached: image_cac24b.jpg, image_cac26c.jpg

## Gemini — 19 Sep 2026, 19:14

Based on the close-up of the 9-pin black connector, here is the sequence of the eight visible colored wires routed into the housing from top to bottom:

*   Black
*   Yellow
*   Blue
*   Green
*   Orange
*   White
*   Brown

.

. I have noted your breakdown that the thick sheathed cable for this plug contains red, black, and orange wires internally.

Whenever you are ready, let me know what these specific wire colors map to down inside the bike frame or on the motherboard.

---

## You — 19 Sep 2026, 19:15

There is a grey wire between the Orange and White.

## Gemini — 19 Sep 2026, 19:15

Good catch, I see it tucked in there now. That means all nine slots on the housing are fully populated.

Here is the corrected sequence for the 9-pin connector from top to bottom:

*   Red
*   Black
*   Yellow
*   Blue
*   Green
*   Orange
*   Grey
*   White
*   Brown

Whenever you are ready, let me know what these bundles connect to on the other end down inside the bike.

---

## You — 19 Sep 2026, 19:17

The three-wire plug actually goes down to the speed or the resistance control knob. This is a picture of the inside of the resistance control knob, and you can see there's the braking cable, but the wire that comes up to that white three-pin plug there goes into the knob. What's interesting about this is the resistance knob is an electric signal. It looks like it is a rotary encoder of some sort, and when I looked at that with a voltage, it seems to have between the red and the black wires, which are the two outside pins, a sort of 3.3 volt signal. Between the red and the middle one, the brown, every click it goes on, and the next click it goes off, 3.3 volts on, then click again it goes to zero, click again it goes to 3.3. So that's between the red and the brown wire, so the middle one. When I measured the red and the black wire, so I think it's red and black that way round, so the two outside wires anyway, what I was getting was every click it flicked high and then off again, and it was hard to register what it was, maybe 1.5 or 2 volts, but I think what it's doing is clicking quite quickly. So it clicks high on a click, and then the next time you turn the knob to another click, it will click high, and that's between the two outside wires. Between the inside wire and the outside wire, which I assume is the red one, is every click on the rotary encoder it goes 3.3 volts, then another click it goes to no volts, then another click it goes to 3.3 volts. That's probably something we should look into and see if it matches rotary encoders that we have access to.

📎 Attached: image_cac669.jpg

## Gemini — 19 Sep 2026, 19:17

The resistance control knob assembly integrates both mechanical and electrical systems. .

Based on your multimeter readings, this component is definitively an **incremental quadrature rotary encoder** operating on a 3.3V logic circuit. Here is how your voltage readings map to standard encoder behavior:

*   **The 3-Wire Interface:** Standard mechanical rotary encoders use three pins: a Common pin (either Ground or 3.3V provided by the motherboard) and two signal lines (Channel A and Channel B).
*   **The Toggle Signal (Red to Middle):** The state change from 0V to 3.3V and holding on alternating clicks indicates one of the signal channels. Depending on how the internal mechanical detents align with the contact pads, resting on a new click will often hold the signal high or low until the next movement.
*   **The Pulse Signal (Red to Black):** The momentary voltage spike (flicking high, then returning to zero) represents the second signal channel making and breaking contact as the knob turns between the physical detents.

Because these two signal channels pulse 90 degrees out of phase, the motherboard reads the overlapping sequence of the pulses to determine both the _amount_ of rotation and the _direction_ (clockwise to increase resistance, counter-clockwise to decrease). The console then translates this digital input into a command sent down the 9-pin loom to the lower resistance motor.

What component from the main body or the 9-pin wiring loom do you want to document next?

---

## You — 19 Sep 2026, 19:20

Okay, I'm going to give you some pictures now of the actual circuit board in the Specialized Turbo Vado, front and the back. It might be interesting to analyze what we can see on this and the maker names and the various sort of little chips and so on. So I'm going to put a bunch of photos on there now.

📎 Attached: image_cace65.jpg, image_cace8c.jpg, image_cb2022.jpg

## Gemini — 19 Sep 2026, 19:20

. This confirms we are definitely still looking at the main console motherboard for the Echelon EX3.

Here is a breakdown of the components and markings visible on this PCB:

. .

. . . , likely used for current limiting or load balancing for devices plugged into that port. . Because the bike is supplied with a 9V power supply, these components act as a buck converter circuit to step the voltage down to 5V for the USB port and 3.3V for the logic/Bluetooth chips. . .

Is there a specific integrated circuit on this board you want to look at closer, or are you ready to move down into the motor and resistance mechanism inside the bike frame?

---

## You — 19 Sep 2026, 19:22

Is there also a 6-pin jtag socket there too you can see?

## Gemini — 19 Sep 2026, 19:22

Yes, good eye! There are a couple of very strong candidates for programming/debug headers on this board. In factory environments, JTAG or UART/SWD programming ports are frequently populated with standard white connectors (like JST headers) so that a programming jig can be easily plugged in during manufacturing.

Based on the photos, here are the likely programming/debug interfaces:

. Given its location away from the main power/sensor inputs, this is highly likely to be the main JTAG or serial programming header for the board's primary microcontroller. . Because it is situated right next to the wireless module, this is very likely a dedicated SWD (Serial Wire Debug) or UART port used specifically to flash the firmware onto the Bluetooth chip.

If you have a logic analyzer or a multimeter, probing those pins while the board boots up might reveal a serial output log or expose the programming pins (VCC, GND, TX, RX, or TCK/TMS/etc.).

What component are we pulling apart next?

---

## You — 19 Sep 2026, 19:24

The five pin socket is for the auxiliary circuit board which has the switch on the Echelon bike. This is the circuit for that. It seems to be a very simple board with just four switches and an LED on it.

📎 Attached: image_cb27e1.jpg, image_cb2806.jpg, image_cb283e.jpg

## Gemini — 19 Sep 2026, 19:24

That perfectly explains the 5-pin header we saw next to the Bluetooth module on the main board! .

Here is the documentation breakdown for this auxiliary switch board:

. . .

*   **Rear Circuitry:** The back of the PCB reveals a very basic circuit. . .

Because there are only 5 wires to handle 4 switches and an LED, the switches likely share a common ground (or VCC) and might use a resistor ladder network to send distinct voltage values back to a single analog pin on the main board, minimizing the number of wires needed in the ribbon cable.

What piece of the EX3 are we documenting next? Do you want to look at the lower resistance mechanism or the cadence monitor?

---

## You — 19 Sep 2026, 19:29

Okay, I'm going to go and give you some images now of the lower resistance and cable mechanism there. That's really where everything happens, and what I found when I opened it is there is a single unit which seems to be a fairly central hub on the wiring loom of the bike. So, the nine-pin connector comes down to that unit, and in that unit are really maybe three things or two things. The main one is there is a potentiometer which is connected to the lever which changes the resistance, and I think what that is there to do is to detect where the resistance is in its range. That potentiometer can turn 360 degrees easily and continue to do that, and it goes from zero to about 5.4 kiloohms, and then it clicks off and then it starts at zero again. That's 180 degrees, then it clicks off, starts at zero again, and goes to 5.4 kiloohms on the next 180 degrees, and it clicks off and then starts again with zero to 5.14 kiloohms. So, as that pin turns, which changes the lever of the resistance, it basically has a double cycle that it can detect. What's also in the unit is a motor, and that motor turns that pin. So, as the motor turns, it moves the resistance up or down, and then the potentiometer can detect where it is, and it seems that that motor is also run at 3.3 volts. So, let me send you some photographs of that unit.

📎 Attached: image_cb32e8.jpg, image_cb3307.jpg, image_cb3327.jpg, image_cb3362.jpg

## Gemini — 19 Sep 2026, 19:29

This central hub is the heart of the EX3's resistance system. Based on your electrical readings and the provided images, here is the documentation for the lower actuator assembly:

. . This is what physically interfaces with the magnetic brake carriage. . . As you discovered, the motherboard drives this motor at 3.3V to mechanically turn the adjustment pin. . Based on your mapping, the motherboard uses this as a continuous-rotation absolute encoder. By reading the dual 180-degree sweep from 0 to 5.4 kΩ, the console always knows the exact physical position of the resistance magnets, regardless of how many times the motor spins. . . . . This prevents the delicate solder joints from fracturing under the heavy vibrations of an indoor cycling session.

If those two smaller white JST connectors route down to the pedal crank, that would be our cadence sensor. Do you have photos of that component or the sensor itself to wrap up the hardware map?

---

## You — 19 Sep 2026, 19:31

Okay, now I'm going to go onto the wiring loom and I'll give you a photo specifically of the three plugs that come out of the central hub which we've just been talking about. So, in this photo there are three looms. There is the nine pin loom. There is the black loom which goes to the wires which are the grey and brown, which are the that's on the white JST connector, and that is the cadence reed switch. And then at the bottom with the red, black and brown wires is the power in. And the red seems to go to the white cable and the black goes to the brown cable coming out of that bottom white JST connector. That is the nine volt input.

📎 Attached: image_cb3763.jpg

## Gemini — 19 Sep 2026, 19:31

This image perfectly maps how the central wiring hub distributes signals and power throughout the lower bike frame. Here is the breakdown of the three key connections visible at this junction:

. . . . . .

Are there any final components to document, like the physical cadence sensor on the crank, or are we ready to start compiling all of this into a single, structured teardown document?

---

## You — 19 Sep 2026, 19:34

Okay, so let me summarize those nine pins on the loom and what each of the colored wires do based on this. So, on the nine-pin loom, the brown wire is ground, the white wire is +9 volts, the gray and orange wires are the cadence sensor, and they run at a 3.3 volt signal. The green, blue, and yellow go to the potentiometer, where the blue is the variable part of the potentiometer, and it seems to range from the total resistance across the potentiometer is 5.4 kiloohms, and the voltage seems to range from about between the yellow and green, which are the two outer sides, from about 1.7 to 1.16 volts when I was measuring it. It seemed strange, but obviously it definitely moves across there. And then the black and the red wires at the top is the motor control for the resistance motor that also runs at 3.3 volts, so that's the actuator for the resistance. So that's the main loom, so the power comes into that loom and goes up to the circuit board. You've got the cadence switch, you've got the resistance measurement, which is the potentiometer, and you've got the motor to set the resistance.

## Gemini — 19 Sep 2026, 19:34

This is a fantastic piece of reverse-engineering. You have successfully mapped the entire central nervous system of the bike.

Here is the definitive pinout mapping for the main 9-pin loom based on your summary, ready for the final documentation:

Wire Color

Component / Function

Electrical Notes & Observations

**Red**

Resistance Motor

Actuator control; operates at 3.3V.

**Black**

Resistance Motor

Actuator control; operates at 3.3V.

**Yellow**

Potentiometer (Outer)

Resistance measurement reference.

**Blue**

Potentiometer (Wiper)

Variable signal; sweeps 0 to 5.4 kΩ per 180° rotation.

**Green**

Potentiometer (Outer)

Resistance measurement reference. Measured 1.16V to 1.7V across yellow/green.

**Orange**

Cadence Sensor

Reed switch circuit; operates on a 3.3V logic signal.

**Grey**

Cadence Sensor

Reed switch circuit; operates on a 3.3V logic signal.

**White**

Main Power (+)

+9V DC supply routing up to the console motherboard.

**Brown**

Main Power (GND)

System ground.

This gives anyone looking to modify the bike, intercept the BLE signals, or replace a dead motherboard exactly what they need to hardwire their own microcontrollers (like an ESP32) directly into the bike's physical hardware.

Are there any other sub-assemblies you want to dump notes on, or should we start structuring all of this into a complete, top-to-bottom teardown guide?

---

## You — 19 Sep 2026, 19:36

So here's my idea. My idea is to intercept the two loom sockets that go into the back of the motherboard, the JST SM 9-pin and the 3-pin JST, and put them onto a board with the ESP32. I'm thinking of a board where you can plug those in from the sensors and pass them on into the back of the circuit board, but also to intercept them. Now the interesting thing is because the resistance is actually electrically actuated, we could augment the intercept over the top and automatically set resistance on this bike. It's possible to do it even though the actual app doesn't allow for it, the capability is there. So when we do look at the Bluetooth implementation, we might want to put that in. I suspect we may need to think through carefully what the pass-through looks like in some of these circumstances and how we work with the actual knob itself, which is sending those rotary encoders through, but it feels that we should probably try and intercept all of that.

## Gemini — 19 Sep 2026, 19:36

This is a brilliant approach. By acting as a Man-in-The-Middle (MITM) between the hardware and the console, you can introduce true ERG mode (auto-resistance) to a bike that natively lacks it, while preserving the original console's functionality.

Because the bike's logic and sensors already run at 3.3V, the ESP32 is the perfect microcontroller for this, as it natively operates on 3.3V logic without needing level shifters for the data lines.

Here are the primary architectural challenges you need to account for on your intercept board:

*   **Power Management (The 9V Drop):** You cannot power the ESP32 directly from the white (+9V) wire. Your intercept board must include a buck converter or linear voltage regulator (like an LM7833 or AMS1117-3.3) to step the 9V supply down to a stable 3.3V for the ESP.
*   **The Motor Drive Constraint:** ESP32 GPIO pins can only source about 40mA of current, which is nowhere near enough to physically spin the DC resistance motor. You have two distinct paths to handle auto-resistance:
    *   _Path A (The Hardware Takeover):_ Intercept the red and black motor wires completely. Use a dedicated 3.3V motor driver IC (like a DRV8833) on your custom board to control the motor. You would then need to feed a "spoofed" potentiometer signal to the original motherboard so it doesn't throw an error when the physical resistance changes without its permission.
    *   _Path B (The Software Spoof):_ Keep the motor directly wired to the motherboard. Instead of driving the motor, the ESP32 injects synthetic quadrature pulses into the 3-pin rotary encoder lines going up to the console. You effectively "trick" the main motherboard into thinking an invisible hand is turning the resistance knob, and the motherboard's own power circuitry will drive the motor for you.
*   **Input Multiplexing (The Knob Override):** If you route the 3-pin physical knob into the ESP32, and the ESP32 routes to the motherboard, you can handle overrides in software. If the user physically turns the knob, the ESP32 can suspend ERG mode, pass the manual clicks up to the console, and then seamlessly resume auto-adjustment when the manual input stops.
*   **Analog Reading the Potentiometer:** You can split the Blue (wiper) line from the potentiometer directly into an ESP32 ADC (Analog-to-Digital Converter) pin. This allows the ESP32 to constantly monitor the exact physical position of the brake carriage in real-time, creating a closed feedback loop for your auto-resistance algorithms.

---