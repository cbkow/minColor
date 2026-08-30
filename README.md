# minColor

**minColor** is an OCIO-based config and a workflow plugin for Adobe After Effects. The plugin and script use AE's native OCIO plugins but provide an easy shortcut toolset to manage media and timeline color space, bypassing AE's "Interpret Media" workflow. 

**Two OCIO-based workflows are included:** 
- **Linear** workflows with common `ACES` and `Blender` staples — a hybrid of ACES 2.0 and Blender 5.2 configs made compatible with After Effects' current legacy OCIO 2.4 setup. These have familiar settings for anyone comfortable with `ACES` or `Blender` setups.
- An **SDR** workflow meant to supplement Adobe's legacy ICC/ICM workflow for daily SDR work. The SDR config is based on a `Rec. 709, Gamma 2.2` working space, with easy transforms to `sRGB` and `Rec. 1886` for viewing and output. 

Windows and macOS-flavored view transforms are provided to counteract differences in how After Effects handles color management in each OS. Use this workaround for proper viewport colors on macOS until Adobe fixes it.

![mC_001.png](images/mC_001.png)

---

## Install

### macOS

Download `minColor-<version>.pkg` from [Releases](https://github.com/cbkow/minColor/releases/latest),
quit After Effects, and run it. Signed and notarized.

### Windows

Download `minColor-<version>.msi` from [Releases](https://github.com/cbkow/minColor/releases/latest), quit After Effects, and run it. The installer is unsigned, so Windows will ask for permission to run it.

A zip is also attached to each release for manual installation; its `README.txt` shows how to install the plugin and script components.

---

## The Basics

1. **Set Up or Migrate an existing project** by selecting a working color space. If you pick ACEScg, the default media color space will be ACEScg.
2. **Interpret the Timeline** is the most useful part of this plugin. It walks through the timeline and matches all files to color spaces based on presets. Edit these presets with the **Matches** button. Click `Add/Update` to save the setting before exiting the submenu.
![mC_009.png](images/mC_009.png)

3. Use **Interpret Footage** to manually change a layer's color space or...
4. Use the **native AE OCIO effects**. The loaded configs will be recognized.
5. Set your **View** Adjustment layer to what you want to see when working. Set your **Render** Adjustment layer to what you want to output when rendering. Use the Apply buttons to toggle between the two.

---

## The Extras

Regular `sRGB, rec.709, rec.1886, ACES`, and `Blender` workflows are supported, and all the view, looks, and media names you expect are present. *You can use these configs without the plugins.*

To make things easier and reduce inconsistencies between AE on macOS and Windows, minColor provides common-name aliases. Essentially, any **Desktop-labeled** **View** or **Render** option is an alias for `sRGB` flows—typical in web/social/direct/tech branding work. Any **View** or **Render** option labeled **"Video”** is better suited for broadcast delivery and standard commercial post-production workflows. They are all aliases for `Rec. 709 gamma 2.4 / Rec. 1886 / BT. 1886`.

### Web/Tech/Direct Motion Graphics 
If you work for tech/web/direct, this will most likely match your normal workflow:
- **Windows Desktop View** or **macOS Desktop View** — depending on what machine you are currently sitting in front of.
- **Desktop Render**

### Commercial and Broadcast
If you are working in post-production in an offline/online flow, you will most likely want:
- **Windows or macOS Desktop View** is what viewers will see on a TV or a professional broadcast monitor in an Online room.
- **Windows or macOS Video Views** are what people will see when they watch videos online or via a desktop video player. *Pick your poison (and lament the state of video today).*
- **Video Render** is what you will be outputting to offline editors and online finishing artists.

---

## Worth Noting

#### ***minColor Edits Project Files Directly***

The AE SDK doesn't currently expose what’s needed to manage color space programmatically, so minColor circumvents the limitation by directly editing project files and reloading them. 

On migration, it backs up the current .aep file to a `_minColor` folder next to the project and timestamps the backup. It opens the project file, manually edits it to set the bundled OCIO config and correct the workspace, then reloads the project. Backups are always available from before any migration — just in case. 

*There are community requests for more API control over the settings we want, so this could change in the future if Adobe allows it.*

## Documentation

- [Using minColor](docs/using.md)
- [Technical overview](docs/overview.md)
- [Building and releasing](docs/building.md)
