
from metpy.plots import SkewT
from metpy.units import units
from netCDF4 import Dataset
import matplotlib.pyplot as plt
import numpy as np

nc = Dataset("skew.nc","r")
pressure = nc.variables["pressure"][:]
temp     = nc.variables["temperature"][:]
dew      = nc.variables["dew_point"][:]

fig = plt.figure(figsize=(9,11))
skew = SkewT(fig, rotation=45)
skew.plot(pressure,temp,'r')
skew.plot(pressure,dew,'g')
skew.plot_dry_adiabats(t0=np.arange(233, 533, 20) * units.K, alpha=0.25, color='orangered')
skew.plot_moist_adiabats(t0=np.arange(230, 400, 5) * units.K,
                         alpha=0.25, color='tab:green')
plt.show()



