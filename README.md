# Fee Scopes 

## Getting Started

Welcome to the repository on all things related to the Featherscope and Kiloscope, two light-weight head-mounted microscopes developed in the Fee lab at MIT! Our intent with this repository is to give you all the information you need to build your own scopes to use in your lab.

The microscopes described in this repository are featured in [this bioRxiv paper](https://www.biorxiv.org/content/10.1101/2021.09.03.458947v1).

A fully functional system is comprised of the microscope itself, a DAQ and acquisition computer, a laser and associated illumination optics, and (optionally) a commutator for long-term recording. The components necessary for each of these subsystems are listed in `main_bom.ods` along with a cost breakdowns.

## Components

### Microscope

Each microscope is built from small optical components mounted inside 3D printed parts. The optics needed for each microscope can be found under the Kiloscope and Featherscope tabs of `main_bom.ods`. The STEP files for the printed parts are located in the `body_components` folder.

We have had success ordering prints from [Rosenberg Industries](https://www.rosenbergindustries.com), a company that specializes in prints for neuroscience implant devices. Parts are printed in Formlabs Grey resin at 25 micron resolution.

Step-by-step instructions for assembling the microscopes are provided in `scope_assembly_instructions.odp`. Most components are secured using UV cure optical cement that can be purchased from [Edmund Optics](https://www.edmundoptics.com/f/norland-optical-adhesives/11818/) or a similar supplier. These instructions are a work in progress, so please don't hesistate to contact us if any of the steps are unclear!

# TODO

-DAQ firmware

-ATtiny firmware

-docs on bonsai module
