# Fee Scopes 

## Getting Started

Welcome to the repository on all things related to the Featherscope and Kiloscope, two light-weight head-mounted microscopes developed in the Fee lab at MIT! Our intent with this repository is to give you all the information you need to build your own scopes to use in your lab.

The microscopes described in this repository are featured in [this bioRxiv paper](https://www.biorxiv.org/content/10.1101/2021.09.03.458947v1).

A fully functional system is comprised of the microscope itself, a DAQ and acquisition computer, a laser and associated illumination optics, and (optionally) a commutator for long-term recording. The components necessary for each of these subsystems are listed in `main_bom.ods` along with a cost breakdowns.

## Microscope

Each microscope is built from small optical components mounted inside 3D printed parts. The optics needed for each microscope can be found under the Kiloscope and Featherscope tabs of `main_bom.ods`. The STEP files for the printed parts are located in the `body_components` folder.

### Aspheric Lenses

The aspheric lens assemblies used in the Kiloscope are taken from replacement rear camera modules for the Samsung Galaxy S9 phone. These modules can be purchased online from 3rd party vendors, however the lenses then need to be carefully removed from the assembled housing. The lens module must first be removed from the sensor PCB using pliers. The outer sheet metal shell surrounding the lens module can then be removed by first sliding a scalpel underneath on all sides to remove mounting glue, and then carefully prying off the shell. The lens will then slide out of the housing. The remaining hardware attached to the lens can be clipped off with precision electronics pliers.

### Prints

We have had success ordering prints from [Rosenberg Industries](https://www.rosenbergindustries.com), a company that specializes in prints for neuroscience implant devices. Parts are printed in Formlabs Grey resin at 25 micron resolution.

### Assembly

Step-by-step instructions for assembling the microscopes are provided in `scope_assembly_instructions.odp`. Most components are secured using UV cure optical cement that can be purchased from [Edmund Optics](https://www.edmundoptics.com/f/norland-optical-adhesives/11818/) or a similar supplier. These instructions are a work in progress, so please don't hesistate to contact us if any of the steps are unclear!

# TODO

-DAQ firmware

-ATtiny firmware

-docs on bonsai module
