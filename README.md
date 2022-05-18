# Fee Scopes 

## Getting Started

Welcome to the repository on all things related to the Featherscope and Kiloscope, two light-weight head-mounted microscopes developed in the Fee lab at MIT. Our intent with this repository is to give you all the information you need to build your own scopes to use in your lab. Please let us know if you find that any aspects of these resources could be improved!

The microscopes described in this repository are featured in [this bioRxiv paper](https://www.biorxiv.org/content/10.1101/2021.09.03.458947v1).

A fully functional system is comprised of the microscope itself, a DAQ and acquisition computer, a laser and associated illumination optics, and (optionally) a commutator for long-term recording. The components necessary for each of these subsystems are listed in `main_bom.ods` along with a cost breakdown.

## Microscope

Each microscope is built from small optical components mounted inside 3D printed parts. The optics needed for each microscope can be found under the Kiloscope and Featherscope tabs of `main_bom.ods`. The STEP files for the printed parts are located in the `body_components` folder. Technical details about the construction process can be found in the Methods section of the paper.

### Aspheric Lenses

The aspheric lens assemblies used in the Kiloscope are taken from replacement rear camera modules for the Samsung Galaxy S9 phone. These modules can be purchased online from 3rd party vendors, however the lenses then need to be carefully removed from the assembled housing. The lens module must first be removed from the sensor PCB using pliers. The outer sheet metal shell surrounding the lens module can then be removed by first sliding a scalpel underneath on all sides to remove mounting glue, and then carefully prying off the shell. The lens will then slide out of the housing. The remaining hardware attached to the lens can be clipped off with precision electronics pliers. Note that the lens material very easy to scratch and difficult to clean, so be careful not to touch the lens faces during this process! When you have extracted the lens, you can store it safely in a PDMS device carrier (we reuse the containers that Edmund Optics ships their prisms in).

### Ground GRIN Lenses

The tube lens used in the Featherscope is a GRIN lens purchased from Edmund Optics that has been ground down to a total length of 1 mm. Ideally we would have GRINTech, the manufacturer of the original lenses, make us custom lenses with the correct length to begin with. However, GRINTech and Inscopix have signed an anticompetitive exclusive distribution agreement for one-photon calcium imaging, so we have to be a little more creative about how we get these components. We purchase lenses from Edmund Optics (part number in BOM) and have them ground down to 1.00 mm by Pioneer Precision Optics, a custom optics company based in Massachusetts.

### Prints

We have had success ordering prints from [Rosenberg Industries](https://www.rosenbergindustries.com), a company based in Minnesota that specializes in prints for neuroscience implant devices. Parts are printed in Formlabs Grey resin at 25 micron resolution.

### Assembly

Step-by-step instructions for assembling the microscopes are provided in `scope_assembly_instructions.odp`. Most components are secured using UV cure optical cement that can be purchased from [Edmund Optics](https://www.edmundoptics.com/f/norland-optical-adhesives/11818/) or a similar supplier. These instructions are a work in progress, so please don't hesistate to contact us if any of the steps are unclear!

# TODO

-DAQ firmware

-ATtiny firmware

-docs on bonsai module
