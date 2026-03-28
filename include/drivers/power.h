/*
 * Machine Power Control
 *
 * Provides controlled system restart and shutdown.  Both functions disable
 * interrupts before acting and never return.
 *
 * Shutdown targets the ACPI / QEMU power-off I/O ports 0x604, 0xB004, and
 * 0x4004, which work under QEMU, Bochs, and VirtualBox.  On bare-metal
 * without ACPI support the system will halt instead of powering off.
 *
 * Restart uses the keyboard controller CPU reset line (port 0x64, command
 * 0xFE), which is supported on all PC-compatible hardware since the 286.
 */

#ifndef POWER_H
#define POWER_H

/*
 * power_restart - Perform a hard system reset.
 *
 * Pulses the keyboard controller CPU reset line.  If the controller does
 * not respond (line busy), the function falls through to an infinite HLT
 * loop.  Never returns.
 */
void power_restart(void);

/*
 * power_shutdown - Power off the machine.
 *
 * Writes to common VM firmware shutdown ports in order.  Falls through to
 * an infinite HLT loop on hardware that does not respond to any of the
 * ports.  Never returns.
 */
void power_shutdown(void);

#endif /* POWER_H */
