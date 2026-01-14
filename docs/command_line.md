# Command Line Interface

## Basic Usage

The simplest possible operation will rebuild nodes in all of the maps in a provided WAD:
```bash
elfbsp example.wad
```

## Ouput separate file
The following will rebuild all of the nodes in a seperate copy of the provided WAD:
```bash
elfbsp example1.wad --output example2.wad
```

## Specific maps
To build only certain maps' nodes, the following option is available:
```bash
elfbsp example.wad --map MAP01
elfbsp example.wad --map MAP01,MAP03,MAP07 # multiple maps can be provided via comma-separation
elfbsp example.wad --map MAP10-MAP11       # or via a hyphen-separated range
elfbsp example.wad --map MAP04,MAP22-MAP25 # or you may combine both
```

## Documentation
For a basic explanation of the main options, type:
```bash
elfbsp --help
```

For a complete options list, and documentation for each one, type:
```bash
elfbsp --doc
```

## Exit Codes

- 0 if OK.
- 1 if nothing was built (no matching maps).
- 2 if one or more maps failed to build properly.
- 3 if a fatal error occurred.
