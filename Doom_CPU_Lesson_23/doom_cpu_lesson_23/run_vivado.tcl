# Run from this directory: vivado -mode batch -source run_vivado.tcl
set here [file dirname [file normalize [info script]]]
create_project -in_memory lesson23
add_files [list "$here/regfile.v" "$here/alu.v" "$here/tiny_core.v"]
add_files -fileset sim_1 "$here/tb.v"
set_property top tb [get_filesets sim_1]
# Change these parameters and rerun to exercise other lanes and waits.
set_property generic {WAIT=0 SOURCE=100 DEST=200} [get_filesets sim_1]
launch_simulation
run all
close_sim
