WorldRulesB

The World is composed of a grid of square cells.

Each cell is from [0,1] full.

It is filled with matter of up to all of the following types: dirt, water, wood, sand, metal, air, or leaf.

The matter in each cell moves according to 2D kinematics.  

It is modeled by a single particle within each cell.  
This is the cell's COM, or Center of Mass.

When the particle crosses from inside a cell boundry to another cell,
the matter will either transfer into the target cell, or it will reflect off 
of the shared boundary, or some of both.

If the target cell has matter but isn't full, the some of the matter can transfer.
Matter is conserved during transfers.

Every type of matter has the following properties:
* elasticity
* cohesion
* adhesion
* density


