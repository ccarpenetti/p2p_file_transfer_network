.PHONY: all clean base64 index nettest

# Define targets for each sub-makefile
all: base64 index nettest

base64:
	$(MAKE) -C base64

index:
	$(MAKE) -C index

nettest:
	$(MAKE) -C nettest

clean:
	$(MAKE) -C base64 clean
	$(MAKE) -C index clean
	$(MAKE) -C nettest clean
