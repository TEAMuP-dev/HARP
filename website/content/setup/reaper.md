# Set Up HARP in REAPER

To link HARP to REAPER as an external sample editor, do the following:

* Select _REAPER > Preferences_ (Mac) or press _control+p_ (Windows).

* Select _External Editors_, click _Add_, then click _Browse_ by _Primary Editor_.

* Select your HARP executable from your chosen installation directory (e.g., `~/Applications/HARP.app`) and confirm.

<p align="center">
   <img width="1023" src="https://github.com/TEAMuP-dev/HARP/assets/33099118/b828a2fd-5378-490c-be37-11f7404eb127">
</p>

### Open Audio with HARP in REAPER

To load an audio region into HARP without leaving the DAW, we can now do the following:

* (Optional) Right-click a region and select _Render items as new take_. HARP will perform __destructive__ edits if you save your outputs, meaning it will overwrite input regions. By rendering as a new take, we create a duplicate region and ensure the original is not affected.

* Right-click the (duplicated) region and select _Open items in editor > Open items in 'HARP.app'_.

<p align="center">
   <img width="1531" src="https://github.com/TEAMuP-dev/HARP/assets/33099118/7f26857f-61de-4765-9671-fb69c4264dc4">
</p>

For more instructions on how to use HARP, see our [Workflow](/content/usage/workflow.html) documentation.
