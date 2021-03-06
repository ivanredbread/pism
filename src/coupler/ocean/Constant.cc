// Copyright (C) 2011, 2012, 2013, 2014, 2015, 2016, 2017 PISM Authors
//
// This file is part of PISM.
//
// PISM is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 3 of the License, or (at your option) any later
// version.
//
// PISM is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License
// along with PISM; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#include <gsl/gsl_math.h>

#include "Constant.hh"
#include "pism/util/Vars.hh"
#include "pism/util/ConfigInterface.hh"
#include "pism/util/IceGrid.hh"
#include "pism/util/pism_options.hh"
#include "pism/util/iceModelVec.hh"
#include "pism/util/error_handling.hh"
#include "pism/util/io/io_helpers.hh"
#include "pism/util/MaxTimestep.hh"
#include "pism/util/pism_utilities.hh"

namespace pism {
namespace ocean {
Constant::Constant(IceGrid::ConstPtr g)
  : OceanModel(g) {

  {
    const double
      Q           = m_config->get_double("ocean.sub_shelf_heat_flux_into_ice"),
      L           = m_config->get_double("constants.fresh_water.latent_heat_of_fusion"),
      ice_density = m_config->get_double("constants.ice.density");

    // Set default melt rate using configuration parameters
    // following has units:   J m-2 s-1 / (J kg-1 * kg m-3) = m s-1
    m_meltrate = Q / (L * ice_density);

    // check the command-line option
    options::Real meltrate("-shelf_base_melt_rate",
                           "Specifies a sub shelf ice-equivalent melt rate in meters year-1",
                           units::convert(m_sys, m_meltrate, "m second-1", "m year-1"));

    // tell the user that we're using it
    if (meltrate.is_set()) {
      m_log->message(2,
                     "    - option '-shelf_base_melt_rate' seen, "
                     "setting basal sub shelf basal melt rate to %.2f m year-1 ... \n",
                     (double)meltrate);
    }

    // here meltrate was set using the command-line option OR the default provided above
    m_meltrate = units::convert(m_sys, meltrate, "m year-1", "m second-1");

    // convert to [kg m-2 s-1] = [m s-1] * [kg m-3]
    m_meltrate = m_meltrate * ice_density;
  }
}

Constant::~Constant() {
  // empty
}

void Constant::update_impl(double my_t, double my_dt) {
  // do nothing
  m_t = my_t;
  m_dt = my_dt;
}

void Constant::init_impl() {

  m_t = m_dt = GSL_NAN;  // every re-init restarts the clock

  if (!m_config->get_boolean("ocean.always_grounded")) {
    m_log->message(2, "* Initializing the constant ocean model...\n");
  }
}

MaxTimestep Constant::max_timestep_impl(double t) const {
  (void) t;
  return MaxTimestep("ocean constant");
}

void Constant::sea_level_elevation_impl(double &result) const{
  result = m_sea_level;
}

void Constant::shelf_base_temperature_impl(IceModelVec2S &result) const {
  const double T0 = m_config->get_double("constants.fresh_water.melting_point_temperature"), // K
    beta_CC       = m_config->get_double("constants.ice.beta_Clausius_Clapeyron"),
    g             = m_config->get_double("constants.standard_gravity"),
    ice_density   = m_config->get_double("constants.ice.density");

  const IceModelVec2S *ice_thickness = m_grid->variables().get_2d_scalar("land_ice_thickness");

  IceModelVec::AccessList list{ice_thickness, &result};

  for (Points p(*m_grid); p; p.next()) {
    const int i = p.i(), j = p.j();
    const double pressure = ice_density * g * (*ice_thickness)(i,j); // FIXME issue #15
    // temp is set to melting point at depth
    result(i,j) = T0 - beta_CC * pressure;
  }
}

//! @brief Computes mass flux in [kg m-2 s-1], from assumption that
//! basal heat flux rate converts to mass flux.
void Constant::shelf_base_mass_flux_impl(IceModelVec2S &result) const {
  result.set(m_meltrate);
}

} // end of namespape ocean
} // end of namespace pism
