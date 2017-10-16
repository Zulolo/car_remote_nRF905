################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: car_remote_nRF905

# Tool invocations
car_remote_nRF905: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross GCC Linker'
	arm-linux-gnueabi-gcc -L/home/zulolo/eclipse-workspace/libs/wiringPi -o "car_remote_nRF905" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(EXECUTABLES)$(OBJS)$(C_DEPS) car_remote_nRF905
	-@echo ' '

post-build:
	-@echo 'deploy executable'
	-scp car_remote_nRF905 pi@192.168.0.121:/home/pi/
	-@echo ' '

.PHONY: all clean dependents post-build

-include ../makefile.targets
