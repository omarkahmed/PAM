rm *.png *.nc
cd ../build
rm driver
./../pam-c/make_debug_script.sh $3
mpirun.mpich -n $3 ./driver ../inputs/pamc_input_extruded_$2.yaml
cd ../pam-c
mv ../build/*.nc .