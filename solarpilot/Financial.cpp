/*******************************************************************************************************
*  Copyright 2017 Alliance for Sustainable Energy, LLC
*
*  NOTICE: This software was developed at least in part by Alliance for Sustainable Energy, LLC
*  (�Alliance�) under Contract No. DE-AC36-08GO28308 with the U.S. Department of Energy and the U.S.
*  The Government retains for itself and others acting on its behalf a nonexclusive, paid-up,
*  irrevocable worldwide license in the software to reproduce, prepare derivative works, distribute
*  copies to the public, perform publicly and display publicly, and to permit others to do so.
*
*  Redistribution and use in source and binary forms, with or without modification, are permitted
*  provided that the following conditions are met:
*
*  1. Redistributions of source code must retain the above copyright notice, the above government
*  rights notice, this list of conditions and the following disclaimer.
*
*  2. Redistributions in binary form must reproduce the above copyright notice, the above government
*  rights notice, this list of conditions and the following disclaimer in the documentation and/or
*  other materials provided with the distribution.
*
*  3. The entire corresponding source code of any redistribution, with or without modification, by a
*  research entity, including but not limited to any contracting manager/operator of a United States
*  National Laboratory, any institution of higher learning, and any non-profit organization, must be
*  made publicly available under this license for as long as the redistribution is made available by
*  the research entity.
*
*  4. Redistribution of this software, without modification, must refer to the software by the same
*  designation. Redistribution of a modified version of this software (i) may not refer to the modified
*  version by the same designation, or by any confusingly similar designation, and (ii) must refer to
*  the underlying software originally provided by Alliance as �System Advisor Model� or �SAM�. Except
*  to comply with the foregoing, the terms �System Advisor Model�, �SAM�, or any confusingly similar
*  designation may not be used to refer to any modified version of this software or any modified
*  version of the underlying software originally provided by Alliance without the prior written consent
*  of Alliance.
*
*  5. The name of the copyright holder, contributors, the United States Government, the United States
*  Department of Energy, or any of their employees may not be used to endorse or promote products
*  derived from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
*  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
*  FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER,
*  CONTRIBUTORS, UNITED STATES GOVERNMENT OR UNITED STATES DEPARTMENT OF ENERGY, NOR ANY OF THEIR
*  EMPLOYEES, BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
*  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
*  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
*  THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************************************/

#include <vector>
#include <math.h>

#include "definitions.h"
#include "Financial.h"
#include "SolarField.h"

using namespace std;

void Financial::Create(var_map &V)
{
    _var_fin = &V.fin;

    updateCalculatedParameters(V);
}

void Financial::updateCalculatedParameters(var_map &V)
{
    //if schedules are not provided in the var_map, create them now
    if( V.fin.pricing_array.Val().size() < 2 )
    {
        //Assign the hourly schedules
	    CreateHourlyTODSchedule(V);
        V.fin.pricing_array.Setval( _pricing_array );
        V.fin.schedule_array.Setval( _schedule_array );
    }
    else
    {
        //the arrays have been created previously and we should copy now.
        
        _pricing_array = V.fin.pricing_array.Val();
        _schedule_array = V.fin.schedule_array.Val();
    }

    calcPlantCapitalCost(V);

    _var_fin->schedule_array.Setval( _schedule_array );
    _var_fin->pricing_array.Setval( _pricing_array );
    _var_fin->tower_cost.Setval( _tower_cost );
    _var_fin->rec_cost.Setval( _rec_cost );
    _var_fin->plant_cost.Setval( _plant_cost );
    _var_fin->tes_cost.Setval( _tes_cost );
    _var_fin->site_cost.Setval( _site_cost );
    _var_fin->heliostat_cost.Setval( _heliostat_cost );
    _var_fin->wiring_cost.Setval( _wiring_cost );
    _var_fin->contingency_cost.Setval( _contingency_cost );
    _var_fin->total_direct_cost.Setval( _total_direct_cost );
    _var_fin->total_indirect_cost.Setval( _total_indirect_cost );
    _var_fin->land_cost.Setval( _land_cost );
    _var_fin->sales_tax_cost.Setval( _sales_tax_cost );
    _var_fin->total_installed_cost.Setval( _total_installed_cost );
    _var_fin->cost_per_capacity.Setval( _cost_per_capacity );
}

std::vector< double >* Financial::getPricingArray()
{
    return &_pricing_array;
}

std::vector< int >* Financial::getScheduleArray()
{
    return &_schedule_array;
}

void Financial::CreateHourlyTODSchedule(var_map &V){
	/* 
	Take a schedule (12x24 = 288) of the TOD factors in string form and convert them into 
	an 8760 schedule of integers indicating the TOD factor for each hour of the year.

	Assume the year starts on a Sunday
	*/

	int nwd = V.fin.weekday_sched.val.size();
	int nwe = V.fin.weekend_sched.val.size();

	if(nwd != 288 || nwe != 288) return;

	int monthlength[] = {31,28,31,30,31,30,31,31,30,31,30,31};

    _schedule_array.resize(8760);
    _pricing_array.resize(8760);

	int h=0, tod;
	int dow = 6;	//M=0, T=1; W=2; Th=3; Fr=4, Sa=5; Su=6. Start on a Sunday
	string ss;
	for(int i=0; i<12; i++){
		for(int j=0; j<monthlength[i]; j++){
			for(int k=0; k<24; k++){
				ss = dow<5 ? V.fin.weekday_sched.val.at(i*24+k) : V.fin.weekend_sched.val.at(i*24+k);
				to_integer(ss, &tod);
				_schedule_array[h] = tod;
                _pricing_array[h] = V.fin.pmt_factors.val.at(tod-1);
				h++;
			}
			dow==6 ? dow = 0 : dow++ ;
		}
	}

}

void Financial::calcPlantCapitalCost(var_map &V){
	    
	double Asf = V.sf.sf_area.Val(); 
    double Arec = V.sf.rec_area.Val(); 

    double power_gross = V.plt.power_gross.Val(); 
    double power_net = V.plt.power_net.Val();
	double Aland = V.land.land_area.Val(); 
	(void*)&Aland;

	_tower_cost =  V.fin.tower_fixed_cost.val * exp(V.sf.tht.val * V.fin.tower_exp.val ) ;
	
	_rec_cost =  V.fin.rec_ref_cost.val * pow( Arec / V.fin.rec_ref_area.val, V.fin.rec_cost_exp.val ) ;
	
	_plant_cost =  V.fin.plant_spec_cost.val * power_gross * 1000. ;
	
    double tescap = power_gross / V.plt.eta_cycle.val * V.plt.hours_tes.val;
	_tes_cost =  tescap * 1000. * V.fin.tes_spec_cost.val ;
	
	
	_site_cost =  V.fin.site_spec_cost.val * Asf ;
	_heliostat_cost =  V.fin.heliostat_spec_cost.val * Asf ;
	_wiring_cost =  V.fin.wiring_user_spec.val * Asf ;

    double tdc =
        _tower_cost + 
        _rec_cost +
        _heliostat_cost +
        _wiring_cost +
        _plant_cost +
        _tes_cost +
        V.fin.fixed_cost.val ;

	_contingency_cost =  V.fin.contingency_rate.val/100. * tdc ;

	_total_direct_cost =  tdc + _contingency_cost ;

	_land_cost =  V.land.land_area.Val() * V.fin.land_spec_cost.val ;		

	_sales_tax_cost =  
        V.fin.sales_tax_rate.val * V.fin.sales_tax_frac.val * _total_direct_cost / 1.e4 ;

	_total_indirect_cost =  
        _sales_tax_cost + _land_cost ;

	_total_installed_cost =  
        _total_direct_cost + _total_indirect_cost ;

	_cost_per_capacity =  
        _total_installed_cost / power_net / 1000. ;

}

//void Financial::calcSimpleCOE(double *enet, int nval){
//	/* 
//	Calculate the cost of energy on a simple capital cost to energy production ratio.
//	This number is not representative of an actual LCOE.
//	
//	enet :: double[nval]
//	*/
//	if(nval > (int)_schedule_array.ncells()) return;
//
//	double
//		prod = 0.,
//		prod_w = 0.;
//
//	for(int i=0; i<nval; i++){
//		double e = enet[i];
//		prod_w += _schedule_array[i] * e;
//		prod += e;
//	}
//
//
//	_simple_coe = _total_installed_cost/prod;
//	_weighted_coe = _total_installed_cost/prod_w;
//
//}
