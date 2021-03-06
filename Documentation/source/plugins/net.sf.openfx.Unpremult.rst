.. _net.sf.openfx.Unpremult:

UnpremultOFX
============

.. figure:: net.sf.openfx.Unpremult.png
   :alt: 

*This documentation is for version 2.0 of UnpremultOFX.*

Divide the selected channels by alpha (or another channel)

If no channel is selected, or the premultChannel is set to None, the image data is left untouched, but its premultiplication state is set to UnPreMultiplied.

See also: http://opticalenquiry.com/nuke/index.php?title=Premultiplication

Inputs
------

+----------+---------------+------------+
| Input    | Description   | Optional   |
+==========+===============+============+
| Source   |               | No         |
+----------+---------------+------------+

Controls
--------

+-------------------+------------------+----------+-----------------+--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| Label (UI Name)   | Script-Name      | Type     | Default-Value   | Function                                                                                                                                                                                   |
+===================+==================+==========+=================+============================================================================================================================================================================================+
| By                | premultChannel   | Choice   | A               | The channel to use for (un)premult.\ **None**: Don't multiply/divide\ **R**: R channel from input\ **G**: G channel from input\ **B**: B channel from input\ **A**: A channel from input   |
+-------------------+------------------+----------+-----------------+--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| Clip Info...      | clipInfo         | Button   | N/A             | Display information about the inputs                                                                                                                                                       |
+-------------------+------------------+----------+-----------------+--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
