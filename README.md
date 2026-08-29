# minColor

**minColor** is an OCIO-based config and a workflow plugin for Adobe After Effects. The plugin and script use AE's native OCIO plugins but provide an easy shortcut toolset to manage media and timeline color space in the timeline, bypassing AE's "Interpret Media" workflow. 

Two OCIO configs are bundled: one for linear workflows with common ACES and Blender staples — a hybrid of ACES 2.0 and Blender 5.2 configs made compatible with After Effects' current legacy OCIO 2.4 setup. The other is an SDR-flavored config meant to supplement Adobe's legacy ICC/ICM workflow for daily SDR work. The SDR config is based on a Rec. 709 gamma 2.2 working space, with easy transforms to sRGB and Rec. 1886 for viewing and output. 

Additionally, Windows- and macOS-flavored view transforms are provided to counteract differences in how After Effects handles color management in each OS. Use this workaround for proper viewport colors on macOS until Adobe fixes it.

![mC_001.png](images/mC_001.png)

## Install

### macOS

Download `minColor-<version>.pkg` from [Releases](https://github.com/cbkow/minColor/releases/latest),
quit After Effects, and run it. Signed and notarized.

### Windows

Download `minColor-<version>.msi` from [Releases](https://github.com/cbkow/minColor/releases/latest), quit After Effects, and run it. The installer is unsigned, so
Windows asks permission to run.

A zip is also attached to each release for manual installation; its `README.txt` shows how to install the plugin and script components.

## The Basics

1. **Set Up or Migrate and existing project** by selecting a working color space. If you pick ACEScg, the default media color space will be ACEScg.
2. **Interpret the Timeline** Is the most useful part of this plugin. It will walk though a timeline and match all files to color spaces based on a presets. Edit these presets with the **Matches** button.
![mC_009.png](images/mC_009.png)

3. Use **Interpret Footage** to manually change a layers color space or...
4. Use the **native AE OCIO effects**. The loaded configs will be recognized.
5. Set your **View** Adjustment layer to what you want to see when working. Set your **Render** Adjustment layer to what you want to output when rendering. Use the Apply buttons to toggle between the two.
   

## Documentation

- [Using minColor](docs/using.md)
- [Technical overview](docs/overview.md)
- [Building and releasing](docs/building.md)
