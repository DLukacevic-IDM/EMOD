/***************************************************************************************************

Copyright (c) 2018 Intellectual Ventures Property Holdings, LLC (IVPH) All rights reserved.

EMOD is licensed under the Creative Commons Attribution-Noncommercial-ShareAlike 4.0 License.
To view a copy of this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode

***************************************************************************************************/

#include "stdafx.h"
#include "Environment.h"
#include "Debug.h"
#include "Infection.h"
#include "InterventionsContainer.h"
#include "ISusceptibilityContext.h"
#include "RANDOM.h"
#include "SimulationConfig.h"
#include "MathFunctions.h"
#include "StrainIdentity.h"

SETUP_LOGGING( "Infection" )

namespace Kernel
{
    // static initializers for config base class
    MortalityTimeCourse::Enum  InfectionConfig::mortality_time_course   =  MortalityTimeCourse::DAILY_MORTALITY;
    DurationDistribution InfectionConfig::incubation_distribution = DurationDistribution( DistributionFunction::NOT_INITIALIZED);
    DurationDistribution InfectionConfig::infectious_distribution = DurationDistribution( DistributionFunction::NOT_INITIALIZED );
    float InfectionConfig::base_infectivity = 1.0f;
    float InfectionConfig::base_mortality = 1.0f;
    bool  InfectionConfig::enable_disease_mortality = false;

    GET_SCHEMA_STATIC_WRAPPER_IMPL(Infection,InfectionConfig)
    BEGIN_QUERY_INTERFACE_BODY(InfectionConfig)
    END_QUERY_INTERFACE_BODY(InfectionConfig)


    bool InfectionConfig::Configure(const Configuration* config)
    {
        initConfigTypeMap("Enable_Disease_Mortality", &enable_disease_mortality, Enable_Disease_Mortality_DESC_TEXT, true, "Simulation_Type", "GENERIC_SIM,VECTOR_SIM,STI_SIM,ENVIRONMENTAL_SIM,MALARIA_SIM,TBHIV_SIM,TYPHOID_SIM,PY_SIM");
        initConfig( "Mortality_Time_Course", mortality_time_course, config, MetadataDescriptor::Enum("mortality_time_course", Mortality_Time_Course_DESC_TEXT, MDD_ENUM_ARGS(MortalityTimeCourse)), "Enable_Disease_Mortality" );
        initConfigTypeMap("Base_Mortality", &base_mortality, Base_Mortality_DESC_TEXT, 0.0f, 1000.0f, 0.001f, "Enable_Disease_Mortality"); // should default change depending on disease?
        initConfigTypeMap("Base_Infectivity", &base_infectivity, Base_Infectivity_DESC_TEXT, 0.0f, 1000.0f, 0.3f, "Simulation_Type", "GENERIC_SIM,VECTOR_SIM,STI_SIM,ENVIRONMENTAL_SIM,TBHIV_SIM,PY_SIM,HIV_SIM");// should default change depending on disease?
       

        // Configure incubation period using depends-on.
        float param1_incubation = 0.0, param2_incubation = 0.0;
        DistributionFunction::Enum incubation_period_function(DistributionFunction::NOT_INITIALIZED);
        initConfig("Incubation_Period_Distribution", incubation_period_function, config, MetadataDescriptor::Enum("Incubation_Period_Distribution", Incubation_Period_Distribution_DESC_TEXT, MDD_ENUM_ARGS(DistributionFunction)));
        initConfigTypeMap("Incubation_Period_Min", &param1_incubation, Incubation_Period_Min_DESC_TEXT, 0.0f, FLT_MAX, 0.0f, "Incubation_Period_Distribution", "UNIFORM_DURATION");
        initConfigTypeMap("Incubation_Period_Max", &param2_incubation, Incubation_Period_Max_DESC_TEXT, 0.0f, FLT_MAX, 1.0f, "Incubation_Period_Distribution", "UNIFORM_DURATION");
        initConfigTypeMap("Incubation_Period_Mean", &param1_incubation, Incubation_Period_Mean_DESC_TEXT, 0.0f, FLT_MAX, 6.0f, "Incubation_Period_Distribution", "GAUSSIAN_DURATION, POISSON_DURATION");
        initConfigTypeMap("Incubation_Period_Std_Dev", &param2_incubation, Incubation_Period_Std_Dev_DESC_TEXT, 0.0f, FLT_MAX, 1.0f, "Incubation_Period_Distribution", "GAUSSIAN_DURATION");
        initConfigTypeMap("Base_Incubation_Period", &param1_incubation, Base_Incubation_Period_DESC_TEXT, 0.0f, FLT_MAX, 6.0f, "Incubation_Period_Distribution", "FIXED_DURATION, EXPONENTIAL_DURATION");
        initConfigTypeMap("Incubation_Period_Log_Mean", &param1_incubation, Incubation_Period_Log_Mean_DESC_TEXT, 0.0f, FLT_MAX, 6.0f, "Incubation_Period_Distribution", "LOG_NORMAL_DURATION");
        initConfigTypeMap("Incubation_Period_Log_Width", &param2_incubation, Incubation_Period_Log_Width_DESC_TEXT, 0.0f, FLT_MAX, 1.0f, "Incubation_Period_Distribution", "LOG_NORMAL_DURATION");
        
        bool bRet_incubation= JsonConfigurable::Configure(config);
        if (bRet_incubation)
        {
            incubation_distribution.SetParameters(incubation_period_function, param1_incubation, param2_incubation);
        }


        // Configure infectious duration using depends-on.
        float param1 = 0.0, param2 = 0.0;
        DistributionFunction::Enum infectious_distribution_function(DistributionFunction::NOT_INITIALIZED);
        initConfig("Infectious_Period_Distribution", infectious_distribution_function, config, MetadataDescriptor::Enum("Infectious_Period_Distribution", Infectious_Period_Distribution_DESC_TEXT, MDD_ENUM_ARGS(DistributionFunction)), "Simulation_Type", "GENERIC_SIM, POLIO_SIM, VECTOR_SIM, ENVIRONMENTAL_SIM, PY_SIM, STI_SIM");
        initConfigTypeMap("Base_Infectious_Period", &param1, Base_Infectious_Period_DESC_TEXT, 0.0f, FLT_MAX, 0.0f, "Infectious_Period_Distribution", "FIXED_DURATION,EXPONENTIAL_DURATION");
        initConfigTypeMap("Infectious_Period_Min", &param1, Infectious_Period_Min_DESC_TEXT, 0.0f, FLT_MAX, 0.0f, "Infectious_Period_Distribution", "UNIFORM_DURATION");
        initConfigTypeMap("Infectious_Period_Max", &param2, Infectious_Period_Max_DESC_TEXT, 0.0f, FLT_MAX, 0.0f, "Infectious_Period_Distribution", "UNIFORM_DURATION");
        initConfigTypeMap("Infectious_Period_Mean", &param1, Infectious_Period_Mean_DESC_TEXT, 0.0f, FLT_MAX, 0.0f, "Infectious_Period_Distribution", "GAUSSIAN_DURATION,POISSON_DURATION");
        initConfigTypeMap("Infectious_Period_Std_Dev", &param2, Infectious_Period_Std_Dev_DESC_TEXT, 0.0f, FLT_MAX, 0.0f, "Infectious_Period_Distribution", "GAUSSIAN_DURATION");

        bool bRet_infectious = JsonConfigurable::Configure(config);
        if(bRet_infectious)
        {
            infectious_distribution.SetParameters(infectious_distribution_function, param1, param2);
        }
        return bRet_incubation && bRet_infectious;
    }

    Infection::Infection()
        : parent(nullptr)
        , suid(suids::nil_suid())
        , duration(0.0f)
        , total_duration(0.0f)
        , incubation_timer(0.0f)
        , infectious_timer(0.0f)
        , infectiousness(0.0f)
        , infectiousnessByRoute()
        , StateChange(InfectionStateChange::None)
        , infection_strain(nullptr)
    {
    }

    BEGIN_QUERY_INTERFACE_BODY(Infection)
        HANDLE_INTERFACE(IInfection)
        HANDLE_ISUPPORTS_VIA(IInfection)
    END_QUERY_INTERFACE_BODY(Infection)

    Infection::Infection(IIndividualHumanContext *context)
        : parent(context)
        , suid(suids::nil_suid())
        , duration(0.0f)
        , total_duration(0.0f)
        , incubation_timer(0.0f)
        , infectious_timer(0.0f)
        , infectiousness(0.0f)
        , infectiousnessByRoute()
        , StateChange(InfectionStateChange::None)
        , infection_strain(nullptr)
    {
    }

    void Infection::Initialize(suids::suid _suid)
    {
        suid = _suid;
    }

    Infection *Infection::CreateInfection(IIndividualHumanContext *context, suids::suid _suid)
    {
        Infection *newinfection = _new_ Infection(context);
        newinfection->Initialize(_suid);

        return newinfection;
    }

    Infection::~Infection()
    {
        delete infection_strain;
    }

    void Infection::SetParameters( IStrainIdentity* infstrain, int incubation_period_override ) // or something
    {
        // Set up infection strain
        CreateInfectionStrain(infstrain);

        if( incubation_period_override != -1 )
        {
            incubation_timer = float(incubation_period_override);
        }
        else
        {
            incubation_timer = InfectionConfig::incubation_distribution.CalculateDuration();
            LOG_DEBUG_F( "incubation_timer initialized to %f for individual %d\n", incubation_timer, parent->GetSuid().data );
        }

        infectious_timer = InfectionConfig::infectious_distribution.CalculateDuration();
        
        LOG_DEBUG_F( "infectious_timer = %f\n", infectious_timer );

        total_duration = incubation_timer + infectious_timer;
        infectiousness = 0;
        StateChange    = InfectionStateChange::None;

        if (incubation_timer <= 0)
        {
            infectiousness = InfectionConfig::base_infectivity;
        }
    }

    void Infection::InitInfectionImmunology(ISusceptibilityContext* _immunity)
    {
    }

    // TODO future : grant access to the susceptibility object by way of the host context and keep the update call neutral
    void Infection::Update(float dt, ISusceptibilityContext* immunity)
    {
        StateChange = InfectionStateChange::None;
        duration += dt;

        if (duration > incubation_timer)
        {
            infectiousness = InfectionConfig::base_infectivity;

            // Used to have a release_assert( infectiousness ) here to make sure infectiousness was not zero, 
            // but setting infectiousness to zero can be a valid use case (e.g., while applying external incidence only)
        }

        // To query for mortality-reducing effects of drugs or vaccines
        IDrugVaccineInterventionEffects* idvie = nullptr;

        // if disease has a daily mortality rate, and disease mortality is on, then check for death. mortality_time_course depends-on enable_disease_mortality BUT DAILY_MORTALITY is default
        if (InfectionConfig::enable_disease_mortality && (InfectionConfig::mortality_time_course == MortalityTimeCourse::DAILY_MORTALITY) && (duration > incubation_timer))
        {
            if ( s_OK != parent->GetInterventionsContext()->QueryInterface(GET_IID(IDrugVaccineInterventionEffects), (void**)&idvie) )
            {
                throw QueryInterfaceException( __FILE__, __LINE__, __FUNCTION__, "parent->GetInterventionsContext()", "IDrugVaccineInterventionEffects", "IIndividualHumanInterventionsContext" );
            }

            if ( randgen->e() < InfectionConfig::base_mortality * dt * immunity->getModMortality() * idvie->GetInterventionReducedMortality() )
            { 
                StateChange = InfectionStateChange::Fatal; 
            }
        }

        if (duration > total_duration)
        {
            // disease mortality active and is accounted for at end of infectious period. mortality_time_course depends-on enable_disease_mortality
            if (InfectionConfig::enable_disease_mortality && InfectionConfig::mortality_time_course == MortalityTimeCourse::MORTALITY_AFTER_INFECTIOUS )
            {
                if ( s_OK != parent->GetInterventionsContext()->QueryInterface(GET_IID(IDrugVaccineInterventionEffects), (void**)&idvie) )
                {
                    throw QueryInterfaceException( __FILE__, __LINE__, __FUNCTION__, "parent->GetInterventionsContext()", "IDrugVaccineInterventionEffects", "IIndividualHumanInterventionsContext" );
                }

                if ( randgen->e() < InfectionConfig::base_mortality * immunity->getModMortality() * idvie->GetInterventionReducedMortality() )
                {
                    StateChange = InfectionStateChange::Fatal;
                }
                else
                {
                    StateChange = InfectionStateChange::Cleared;
                }//  For debugging only  (1-base_mortality) recover, rest chance die at end of infection, modified by mortality immunity
            }
            else
            {
                StateChange = InfectionStateChange::Cleared;
            }
        }

        EvolveStrain(immunity, dt); // genomic modifications
    }

    void Infection::CreateInfectionStrain(IStrainIdentity* infstrain)
    {
        if (infection_strain == nullptr)
        {
            // this infection is new, not passed from another processor, so need to initialize the strain object
            infection_strain = _new_ Kernel::StrainIdentity;
        }

        if (infstrain != nullptr)
        {
            infection_strain->SetAntigenID( infstrain->GetAntigenID() );
            infection_strain->SetGeneticID( infstrain->GetGeneticID() );
            // otherwise, using the default antigenID and substrainID from the StrainIdentity constructor
        }
    }

    void Infection::EvolveStrain(ISusceptibilityContext* immunity, float dt)
    {
        // genetic evolution happens here.
        // infection_strain
    }

    void Infection::GetInfectiousStrainID( IStrainIdentity* infstrain ) 
    {
        // Really want to make this cloning an internal StrainIdentity function
        infstrain->SetAntigenID( infection_strain->GetAntigenID() );
        infstrain->SetGeneticID( infection_strain->GetGeneticID() );
    }

    void Infection::SetContextTo(IIndividualHumanContext* context) { parent = context; }

    IIndividualHumanContext* Infection::GetParent() { return parent; }

    suids::suid Infection::GetSuid() const { return suid; }

    const SimulationConfig* Infection::params() { return GET_CONFIGURABLE(SimulationConfig); } // overridden in derived classes but with different return types to hide the casting operation

    InfectionStateChange::_enum Infection::GetStateChange() const { return StateChange; }

    float Infection::GetInfectiousness() const { return infectiousness; }

    float Infection::GetInfectiousnessByRoute( const string& route ) const {
        if( infectiousnessByRoute.find( route ) == infectiousnessByRoute.end() )
        {
            throw BadMapKeyException( __FILE__, __LINE__, __FUNCTION__, "infectiousnesssByRoute", route.c_str() );
        }
        return infectiousnessByRoute.at(route); 
    }

    // Created for TB, but makes sense to be in base class, but no-one else is using yet, placeholder functionality
    bool Infection::IsActive() const
    {
        return false;
    }

    NonNegativeFloat
    Infection::GetDuration()
    const
    {
        return duration;
    }

    REGISTER_SERIALIZABLE(Infection);

    void Infection::serialize(IArchive& ar, Infection* obj)
    {
        Infection& infection = *obj;
        ar.labelElement("suid") & infection.suid;
        ar.labelElement("duration") & infection.duration;
        ar.labelElement("total_duration") & infection.total_duration;
        ar.labelElement("incubation_timer") & infection.incubation_timer;
        ar.labelElement("infectious_timer") & infection.infectious_timer;
        ar.labelElement("infectiousness") & infection.infectiousness;
        ar.labelElement("infectiousnessByRoute") & infection.infectiousnessByRoute;
        ar.labelElement("StateChange") & (uint32_t&)infection.StateChange;
        ar.labelElement( "infection_strain" ); StrainIdentity::serialize( ar, infection.infection_strain );
    }

    bool
    Infection::StrainMatches( IStrainIdentity * pStrain )
    {
        return( infection_strain->GetAntigenID() == pStrain->GetAntigenID() );
    }
}
