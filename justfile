default: build

build:
    make

run *args:
    make
    sudo ./build/limen {{args}}

bear:
    make clean
    bear -- make

# Network namespace + veth helpers
veth-setup:
    ./scripts/netns-veth.sh create

veth-status:
    ./scripts/netns-veth.sh status

veth-clean:
    ./scripts/netns-veth.sh delete
