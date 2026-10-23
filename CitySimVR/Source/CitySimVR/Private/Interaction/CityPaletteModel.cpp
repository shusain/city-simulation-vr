#include "Interaction/CityPaletteModel.h"

TArray<FCityPaletteEntry> FCityPaletteModel::Entries(const ECityPalettePage Page)
{
	switch (Page)
	{
	case ECityPalettePage::MainMenu:
		return {{ECityPaletteAction::ContinueCity, TEXT("CONTINUE"),
		         TEXT("Return to the current city; time stays as you left it")},
		        {ECityPaletteAction::ReviewLoad, TEXT("LOAD"), TEXT("Review before loading the last local city save")},
		        {ECityPaletteAction::OpenNewCitySetup, TEXT("NEW CITY"),
		         TEXT("Choose difficulty and fresh terrain for a separate city save")},
		        {ECityPaletteAction::OpenSettings, TEXT("SETTINGS"), TEXT("Lighting, controls and game settings")}};
	case ECityPalettePage::NewCitySetup:
		return {
		    {ECityPaletteAction::DifficultyRelaxed, TEXT("RELAXED"),
		     TEXT("More funds, later incidents and cheaper repairs")},
		    {ECityPaletteAction::DifficultyNormal, TEXT("NORMAL"), TEXT("Current baseline rules and starting funds")},
		    {ECityPaletteAction::DifficultyChallenging, TEXT("CHALLENGING"),
		     TEXT("Fewer funds, earlier incidents and costlier repairs")},
		    {ECityPaletteAction::CycleDisasters, TEXT("DISASTERS"),
		     TEXT(
		         "Cycle Off / Low / Medium / High for the new city; ordinary building fires use separate safety rules")},
		    {ECityPaletteAction::RerollTerrain, TEXT("NEW TERRAIN"),
		     TEXT("Choose another seed; current city stays unchanged")},
		    {ECityPaletteAction::CycleMapSize, TEXT("MAP SIZE"),
		     TEXT("Choose 64 x 64 or 80 x 80; larger cities require Desktop")},
		    {ECityPaletteAction::ReviewCreate, TEXT("REVIEW"), TEXT("Review before creating a separate city save")},
		    {ECityPaletteAction::Back, TEXT("CANCEL"), TEXT("Discard setup and return to main menu")}};
	case ECityPalettePage::NewCityReview:
		return {{ECityPaletteAction::ConfirmCreate, TEXT("CREATE CITY"),
		         TEXT("Save a fresh city, then replace the current view")},
		        {ECityPaletteAction::SaveAndBack, TEXT("SAVE & BACK"),
		         TEXT("Save the current city to its own slot before switching")},
		        {ECityPaletteAction::Back, TEXT("CANCEL"), TEXT("Return to setup without creating a city")}};
	case ECityPalettePage::LoadReview:
		return {
		    {ECityPaletteAction::PreviousCity, TEXT("PREVIOUS"), TEXT("Choose the previous saved city")},
		    {ECityPaletteAction::NextCity, TEXT("NEXT"), TEXT("Choose the next saved city")},
		    {ECityPaletteAction::ConfirmLoad, TEXT("LOAD SAVED"),
		     TEXT("Replace the current city with its last local save; unsaved changes are discarded")},
		    {ECityPaletteAction::SaveAndBack, TEXT("SAVE & BACK"),
		     TEXT("Save the current city to its own slot and return without loading")},
		    {ECityPaletteAction::ToggleArchivedCities, TEXT("ARCHIVED"),
		     TEXT("Switch between active and archived city lists")},
		    {ECityPaletteAction::RefreshCityLibrary, TEXT("REFRESH"),
		     TEXT("Rediscover documents and rebuild the local catalog")},
		    {ECityPaletteAction::ReviewArchive, TEXT("ARCHIVE"),
		     TEXT("Review moving this city into the recoverable archive")},
		    {ECityPaletteAction::RestoreCity, TEXT("RESTORE"), TEXT("Return an archived city to the active library")},
		    {ECityPaletteAction::Back, TEXT("CANCEL"), TEXT("Keep the current city")}};
	case ECityPalettePage::ArchiveReview:
		return {{ECityPaletteAction::ConfirmArchive, TEXT("ARCHIVE CITY"),
		         TEXT("Move the saved city and its backup to the archive; restore later")},
		        {ECityPaletteAction::Back, TEXT("CANCEL"), TEXT("Return to the library without moving files")}};
	case ECityPalettePage::Build:
		return {
		    {ECityPaletteAction::OpenRoads, TEXT("ROADS"), TEXT("Build or upgrade a road class")},
		    {ECityPaletteAction::OpenPowerFacilities, TEXT("POWER"),
		     TEXT("Choose basic, coal, oil, wind, solar or nuclear generation")},
		    {ECityPaletteAction::OpenWaterFacilities, TEXT("WATER"),
		     TEXT("Choose a cheap tower or expandable treatment facility")},
		    {ECityPaletteAction::SelectPolice, TEXT("POLICE"), TEXT("Place police: road, power and water required")},
		    {ECityPaletteAction::SelectFire, TEXT("FIRE"), TEXT("Place fire station: road, power and water required")},
		    {ECityPaletteAction::OpenParks, TEXT("PARKS"),
		     TEXT("Choose pocket, neighborhood or city park and view appeal coverage")},
		    {ECityPaletteAction::OpenEducationFacilities, TEXT("EDUCATION"),
		     TEXT("Choose Primary, Secondary or Higher education")},
		    {ECityPaletteAction::SelectHospital, TEXT("HOSPITAL"),
		     TEXT("Treatment facility: Secondary staff, roads, power and water")},
		    {ECityPaletteAction::OpenWasteFacilities, TEXT("WASTE"),
		     TEXT("Choose waste collection or advanced recycling")},
		    {ECityPaletteAction::OpenTechnologyFacilities, TEXT("TECHNOLOGY"),
		     TEXT("Choose a digital exchange, computing center or research campus")},
		    {ECityPaletteAction::Back, TEXT("<"), TEXT("Back to main tools"), true}};
	case ECityPalettePage::TechnologyFacilities:
		return {{ECityPaletteAction::SelectDigitalExchange, TEXT("CONNECTIVITY"),
		         TEXT("Digital exchange: Secondary+ staff; review current operating prerequisites")},
		        {ECityPaletteAction::SelectComputingCenter, TEXT("COMPUTING"),
		         TEXT("Computing center: Secondary+ staff; requires an operating exchange")},
		        {ECityPaletteAction::SelectResearchCampus, TEXT("RESEARCH"),
		         TEXT("Research campus: Higher staff; requires an operating computing center")},
		        {ECityPaletteAction::SelectAIInstitute, TEXT("AI INSTITUTE"), TEXT("Higher staff; operating research enables AI deployment")},
                {ECityPaletteAction::SelectEnterpriseAICenter, TEXT("ENTERPRISE AI"), TEXT("Higher staff; supports optional commercial pilot")},
                {ECityPaletteAction::Back, TEXT("<"), TEXT("Back to Build"), true}};
    case ECityPalettePage::CommercialAi:
        return {{ECityPaletteAction::EnableCommercialAi, TEXT("START PILOT"), TEXT("At next funded daily settlement; two shops per center")},
                {ECityPaletteAction::DisableCommercialAi, TEXT("PAUSE PILOT"), TEXT("Restore ordinary roles; prepaid contract retained without refund")},
                {ECityPaletteAction::OverlayCommercialAi, TEXT("PILOT VIEW"), TEXT("See actual participating shops; point for exact paid/staff/outage status")},
                {ECityPaletteAction::Back, TEXT("BACK"), TEXT("Back to Technology")}};
	case ECityPalettePage::TechnologyReview:
		return {{ECityPaletteAction::ReviewDigitalExchange, TEXT("CONNECTIVITY"),
		         TEXT("Review exchange requirements and operation")},
		        {ECityPaletteAction::ReviewComputingCenter, TEXT("COMPUTING"),
		         TEXT("Review computing requirements and operation")},
		        {ECityPaletteAction::ReviewResearchCampus, TEXT("RESEARCH"),
		         TEXT("Review research requirements and operation")},
		        {ECityPaletteAction::ReviewAIInstitute, TEXT("AI INSTITUTE"), TEXT("Review institute prerequisites and operation")},
                {ECityPaletteAction::ReviewEnterpriseAICenter, TEXT("ENTERPRISE AI"), TEXT("Review optional commercial access centers")},
                {ECityPaletteAction::OpenCommercialAi, TEXT("AI PILOT"), TEXT("Review real software fees, roles and retail capacity")},
                {ECityPaletteAction::Back, TEXT("BACK"), TEXT("Back to City")}};
	case ECityPalettePage::WasteFacilities:
		return {{ECityPaletteAction::SelectWasteDepot, TEXT("WASTE DEPOT"),
		         TEXT("Collection trucks and disposal; Basic staff, roads, power and water")},
		        {ECityPaletteAction::SelectRecycling, TEXT("RECYCLING"),
		         TEXT("Secondary staff; collect and divert60% of processed waste")},
		        {ECityPaletteAction::Back, TEXT("<"), TEXT("Back to Build"), true}};
	case ECityPalettePage::EducationFacilities:
		return {
		    {ECityPaletteAction::SelectPrimarySchool, TEXT("PRIMARY"), TEXT("None to Basic; road, power and water")},
		    {ECityPaletteAction::SelectSecondarySchool, TEXT("SECONDARY"),
		     TEXT("Basic to Secondary; road, power and water")},
		    {ECityPaletteAction::SelectCollege, TEXT("HIGHER"), TEXT("Secondary to Higher; road, power and water")},
		    {ECityPaletteAction::Back, TEXT("<"), TEXT("Back to Build"), true}};
	case ECityPalettePage::PowerFacilities:
		return {{ECityPaletteAction::SelectPower, TEXT("BASIC"),
		         TEXT("Existing expandable plant; supply200; unstaffed floor100")},
		        {ECityPaletteAction::SelectCoalPower, TEXT("COAL"), TEXT("supply400; no education; pollution35")},
		        {ECityPaletteAction::SelectOilPower, TEXT("OIL"), TEXT("supply600; pollution20")},
		        {ECityPaletteAction::SelectWindPower, TEXT("WIND"), TEXT("supply150; no pollution")},
		        {ECityPaletteAction::SelectSolarPower, TEXT("SOLAR"), TEXT("supply300; no pollution")},
		        {ECityPaletteAction::SelectNuclearPower, TEXT("NUCLEAR"), TEXT("supply2000; no air pollution")},
		        {ECityPaletteAction::Back, TEXT("<"), TEXT("Back to Build"), true}};
	case ECityPalettePage::WaterFacilities:
		return {{ECityPaletteAction::SelectWaterTower, TEXT("TOWER"), TEXT("supply100; no education; connected road")},
		        {ECityPaletteAction::SelectService, TEXT("TREATMENT"),
		         TEXT("Existing expandable treatment; supply200; unstaffed floor100")},
		        {ECityPaletteAction::Back, TEXT("<"), TEXT("Back to Build"), true}};
	case ECityPalettePage::Parks:
		return {{ECityPaletteAction::SelectPark, TEXT("POCKET"), TEXT("3x3; +20 appeal within8 cells; connected road")},
		        {ECityPaletteAction::SelectNeighborhoodPark, TEXT("NEIGHBORHOOD"),
		         TEXT("5x5; +25 appeal within12 cells; road, power4/water4")},
		        {ECityPaletteAction::SelectCityPark, TEXT("CITY PARK"),
		         TEXT("7x7; +30 appeal within18 cells; road, power8/water8")},
		        {ECityPaletteAction::SelectBasketballPark, TEXT("BASKETBALL"),
		         TEXT("5x5; medium park appeal and utility requirements")},
		        {ECityPaletteAction::SelectSkatePark, TEXT("SKATE PARK"),
		         TEXT("5x5; medium park appeal and utility requirements")},
		        {ECityPaletteAction::SelectBaseballPark, TEXT("BASEBALL"),
		         TEXT("7x7; large park appeal and utility requirements")},
		        {ECityPaletteAction::SelectSoccerPark, TEXT("SOCCER"),
		         TEXT("7x7; large park appeal and utility requirements")},
		        {ECityPaletteAction::OverlayParks, TEXT("PARK VIEW"),
		         TEXT("Exact operating park appeal; strongest benefit wins; cap30")},
		        {ECityPaletteAction::Back, TEXT("<"), TEXT("Back to Build"), true}};
	case ECityPalettePage::Roads:
		return {
		    {ECityPaletteAction::SelectDirtRoad, TEXT("DIRT  $4"), TEXT("Cheap two-way road; slow and low capacity")},
		    {ECityPaletteAction::SelectLocalRoad, TEXT("LOCAL  $10"),
		     TEXT("Two-lane street with walking and building access")},
		    {ECityPaletteAction::SelectAvenue, TEXT("AVENUE  $30"),
		     TEXT("Four-lane arterial with higher speed and capacity")},

		    {ECityPaletteAction::SelectHighway, TEXT("HIGHWAY  $60"),
		     TEXT("Fast limited-access road; no walking or direct frontage")},
		    {ECityPaletteAction::Back, TEXT("<"), TEXT("Back to build tools"), true}};
	case ECityPalettePage::Zone:
		return {
		    {ECityPaletteAction::SelectResidentialZone, TEXT("RESIDENTIAL"), TEXT("Paint residential zoning (green)")},
		    {ECityPaletteAction::SelectCommercialZone, TEXT("COMMERCIAL"), TEXT("Paint commercial zoning (blue)")},
		    {ECityPaletteAction::SelectIndustrialZone, TEXT("INDUSTRIAL"),
		     TEXT("Paint industrial zoning (yellow); creates jobs and pollution")},
		    {ECityPaletteAction::Back, TEXT("<"), TEXT("Back to main tools"), true}};
	case ECityPalettePage::City:
		return {
		    {ECityPaletteAction::OpenTechnology, TEXT("TECHNOLOGY"),
		     TEXT("Review operating connectivity, computing and research progression")},
            {ECityPaletteAction::OpenPolicies, TEXT("POLICIES"), TEXT("Fund resident assistance and school bursaries")},
            {ECityPaletteAction::OpenLoans, TEXT("CITY LOANS"), TEXT("Review civic lender terms, debt and repayments")},
            {ECityPaletteAction::OpenQuarterlyNews, TEXT("CITY NEWS"), TEXT("Quarterly citizen sentiment, concerns, housing and finances")},
		    {ECityPaletteAction::OverlayHappiness, TEXT("HAPPINESS"),
		     TEXT("Recorded citizen mood by home; point for concerns and search status")},
		    {ECityPaletteAction::OpenHousingHistory, TEXT("HOUSING"),
		     TEXT("Current search warnings and recent household moves/departures")},
		    {ECityPaletteAction::OverlayCrime, TEXT("CRIME VIEW"),
		     TEXT("Crime pressure, job gaps, homelessness and police protection")},
		    {ECityPaletteAction::OverlayFire, TEXT("FIRE VIEW"),
		     TEXT("Fire risk, burning/damaged lots and protection")},
		    {ECityPaletteAction::OverlayWaste, TEXT("WASTE"), TEXT("Accumulated waste, caps and collection needs")},
		    {ECityPaletteAction::OverlayHealth, TEXT("HEALTH"),
		     TEXT("Ill residents by home; inspect hospitals for treatment capacity")},
		    {ECityPaletteAction::OverlayEducation, TEXT("EDUCATION"),
		     TEXT("School access, capacity and education progress")},
		    {ECityPaletteAction::OverlayParks, TEXT("PARK VIEW"),
		     TEXT("Operating park appeal; strongest overlapping benefit wins")},
		    {ECityPaletteAction::OpenReports, TEXT("REPORTS"), TEXT("Settled history: hourly, daily, five-day weekly and monthly charts")},
		    {ECityPaletteAction::OpenTaxes, TEXT("TAXES"), TEXT("Adjust residential, commercial and industrial taxes")},
		    {ECityPaletteAction::Back, TEXT("<"), TEXT("Back to main tools"), true}};
	case ECityPalettePage::Reports:
		return {{ECityPaletteAction::ReportHourly, TEXT("HOURLY"), TEXT("Real hourly observations; financial bills still settle daily")},
		        {ECityPaletteAction::ReportDaily, TEXT("DAILY"), TEXT("Each settled day; no partial-day estimates")},
		        {ECityPaletteAction::ReportWeekly, TEXT("WEEKLY"), TEXT("Five-day weeks; observed totals and closing snapshots")},
		        {ECityPaletteAction::ReportMonthly, TEXT("MONTHLY"), TEXT("25-day months; default view")},
		    {ECityPaletteAction::ReportPeople, TEXT("PEOPLE"),
		     TEXT("Population, housing, jobs and resident education")},
		    {ECityPaletteAction::ReportStaff, TEXT("STAFF"),
		     TEXT("Filled civic/private jobs and staff qualifications")},
		    {ECityPaletteAction::ReportMoney, TEXT("FINANCES"), TEXT("Settled treasury, income, upkeep and wages")},
		    {ECityPaletteAction::ReportGoods, TEXT("GOODS"), TEXT("Production, completed purchases/exports and stock")},
		    {ECityPaletteAction::ReportHealth, TEXT("HEALTH"),
		     TEXT("Illness, treatment and hospital capacity history")},
		    {ECityPaletteAction::ReportWaste, TEXT("WASTE"),
		     TEXT("Waste stocks and cumulative processing/diversion")},
		    {ECityPaletteAction::ReportUtilities, TEXT("UTILITIES"),
		     TEXT("Latest daily closing demand, capacity and pollution")},
		    {ECityPaletteAction::ReportMetricPrevious, TEXT("< METRIC"), TEXT("Previous metric in this category")},
		    {ECityPaletteAction::ReportMetricNext, TEXT("METRIC >"), TEXT("Next metric in this category")},
		    {ECityPaletteAction::ReportPeriodPrevious, TEXT("< PERIOD"), TEXT("Previous observed period")},
		    {ECityPaletteAction::ReportPeriodNext, TEXT("PERIOD >"), TEXT("Next observed period")},
		    {ECityPaletteAction::Back, TEXT("BACK"), TEXT("Return to city tools; release grip to dismiss")}};
	case ECityPalettePage::Taxes:
		return {{ECityPaletteAction::TaxResidentialDown, TEXT("R -"), TEXT("Reduce residential tax by 1%")},
		        {ECityPaletteAction::TaxResidentialUp, TEXT("R +"), TEXT("Increase residential tax by 1%")},
		        {ECityPaletteAction::TaxCommercialDown, TEXT("C -"), TEXT("Reduce commercial tax by 1%")},
		        {ECityPaletteAction::TaxCommercialUp, TEXT("C +"), TEXT("Increase commercial tax by 1%")},
		        {ECityPaletteAction::TaxResidentialTrack, TEXT("R TRACK"),
		         TEXT("Hold trigger and sweep across for a coarse residential rate")},
		        {ECityPaletteAction::TaxCommercialTrack, TEXT("C TRACK"),
		         TEXT("Hold trigger and sweep across for a coarse commercial rate")},
		        {ECityPaletteAction::TaxIndustrialDown, TEXT("I -"), TEXT("Reduce industrial tax by 1%")},
		        {ECityPaletteAction::TaxIndustrialUp, TEXT("I +"), TEXT("Increase industrial tax by 1%")},
		        {ECityPaletteAction::TaxIndustrialTrack, TEXT("I TRACK"),
		         TEXT("Hold trigger and sweep across to adjust industrial tax")},
		        {ECityPaletteAction::ApplyTaxes, TEXT("APPLY"), TEXT("Commit all three pending tax rates")},
		        {ECityPaletteAction::CancelTaxes, TEXT("CANCEL"), TEXT("Discard pending tax changes")}};
    case ECityPalettePage::Policies:
        return {{ECityPaletteAction::ReviewReservePolicy,TEXT("RESERVE AID"),TEXT("Review enabling/disabling means-tested purchase reserves")},
                {ECityPaletteAction::ReviewBursaryPolicy,TEXT("BURSARIES"),TEXT("Review cash bursaries for served school learners")},
                {ECityPaletteAction::OpenLoans,TEXT("LOANS"),TEXT("Loan terms and debt service")},
                {ECityPaletteAction::NewsMoney,TEXT("FINANCES"),TEXT("Settled policy costs and recipient payments")},
                {ECityPaletteAction::Back,TEXT("BACK"),TEXT("Return to city tools")}};
    case ECityPalettePage::PolicyReview:
        return {{ECityPaletteAction::ConfirmPolicy,TEXT("CONFIRM"),TEXT("Apply the displayed policy choice for the next settlement")},
                {ECityPaletteAction::Back,TEXT("CANCEL"),TEXT("Leave the existing policy unchanged")}};
    case ECityPalettePage::Loans:
        return {{ECityPaletteAction::ReviewLoanSmall, TEXT("$5,000"), TEXT("Review a $5,000 loan; no money moves yet")},
                {ECityPaletteAction::ReviewLoanMedium, TEXT("$10,000"), TEXT("Review a $10,000 loan")},
                {ECityPaletteAction::ReviewLoanLarge, TEXT("$20,000"), TEXT("Review a $20,000 loan")},
                {ECityPaletteAction::ReviewLoanPayoff, TEXT("PAY OFF"), TEXT("Review full repayment; no early-payment fee")},
                {ECityPaletteAction::NewsMoney, TEXT("FINANCES"), TEXT("Recorded loan balances and monthly transfers")},
                {ECityPaletteAction::Back, TEXT("BACK"), TEXT("Return to city tools")}};
    case ECityPalettePage::LoanReview:
        return {{ECityPaletteAction::ConfirmLoan, TEXT("CONFIRM"), TEXT("Commit the displayed loan or full payoff")},
                {ECityPaletteAction::Back, TEXT("CANCEL"), TEXT("Return without borrowing or repayment")}};
    case ECityPalettePage::QuarterlyNews:
        return {{ECityPaletteAction::NewsOlder, TEXT("< OLDER"), TEXT("Previous recorded quarter")},
                {ECityPaletteAction::NewsNewer, TEXT("NEWER >"), TEXT("Next recorded quarter")},
                {ECityPaletteAction::OverlayHappiness, TEXT("HAPPINESS"), TEXT("Inspect current home mood and concerns")},
                {ECityPaletteAction::OpenHousingHistory, TEXT("HOUSING"), TEXT("Who moved or left, and why")},
                {ECityPaletteAction::NewsMoney, TEXT("FINANCES"), TEXT("Inspect recorded money flows")},
                {ECityPaletteAction::OpenTaxes, TEXT("TAXES"), TEXT("Review pending rates before applying")},
                {ECityPaletteAction::OpenBuild, TEXT("SERVICES"), TEXT("Build utilities, schools, health, safety and waste services")},
                {ECityPaletteAction::Back, TEXT("BACK"), TEXT("Return to city tools")}};
	case ECityPalettePage::HousingHistory:
		return {{ECityPaletteAction::HousingOlder, TEXT("< OLDER"), TEXT("Previous two recorded housing events")},
		        {ECityPaletteAction::HousingNewer, TEXT("NEWER >"), TEXT("Next two recorded housing events")},
		        {ECityPaletteAction::OverlayHappiness, TEXT("HAPPINESS"),
		         TEXT("Show household mood and home-search badges")},
		        {ECityPaletteAction::Back, TEXT("BACK"), TEXT("Return to city tools")}};
	case ECityPalettePage::DisasterHistory:
		return {{ECityPaletteAction::ReportPeriodPrevious, TEXT("< EVENT"), TEXT("Previous recorded warning/event")},
		        {ECityPaletteAction::ReportPeriodNext, TEXT("EVENT >"), TEXT("Next recorded warning/event")},
		        {ECityPaletteAction::Back, TEXT("BACK"), TEXT("Return to natural disaster settings")}};
	case ECityPalettePage::Disasters:
		return {{ECityPaletteAction::ReportDisasters, TEXT("EVENT HISTORY"),
		         TEXT("Saved event cause, date and affected building count; last16 warnings/events")},
		        {ECityPaletteAction::DisastersOff, TEXT("OFF"),
		         TEXT("Stop randomized earthquakes/tornadoes; building fires continue")},
		        {ECityPaletteAction::DisastersLow, TEXT("LOW"),
		         TEXT("Natural disaster checks every24 days after establishment grace")},
		        {ECityPaletteAction::DisastersMedium, TEXT("MEDIUM"),
		         TEXT("Natural disaster checks every12 days after establishment grace")},
		        {ECityPaletteAction::DisastersHigh, TEXT("HIGH"),
		         TEXT("Natural disaster checks every6 days after establishment grace")},
		        {ECityPaletteAction::Back, TEXT("<"), TEXT("Back to game settings"), true}};
	case ECityPalettePage::Audio:
		return {{ECityPaletteAction::AlarmMute, TEXT("FX MUTE"), TEXT("Mute response sirens and notification effects")},
		        {ECityPaletteAction::Alarm25, TEXT("FX25%"), TEXT("Quiet effects volume")},
		        {ECityPaletteAction::Alarm50, TEXT("FX50%"), TEXT("Medium effects volume")},
		        {ECityPaletteAction::Alarm75, TEXT("FX75%"), TEXT("Default effects volume")},
		        {ECityPaletteAction::Alarm100, TEXT("FX100%"), TEXT("Full effects volume")},
		        {ECityPaletteAction::AmbienceMute, TEXT("AMBIENCE MUTE"),
		         TEXT("Mute area, wildlife, traffic and weather ambience")},
		        {ECityPaletteAction::Ambience25, TEXT("AMBIENCE25%"), TEXT("Quiet city ambience")},
		        {ECityPaletteAction::Ambience50, TEXT("AMBIENCE50%"), TEXT("Default city ambience")},
		        {ECityPaletteAction::Ambience75, TEXT("AMBIENCE75%"), TEXT("Louder city ambience")},
		        {ECityPaletteAction::Ambience100, TEXT("AMBIENCE100%"), TEXT("Full city ambience")},
		        {ECityPaletteAction::MusicMute, TEXT("MUSIC MUTE"), TEXT("Mute music and hold the playlist position")},
		        {ECityPaletteAction::Music25, TEXT("MUSIC25%"), TEXT("Default quiet music volume")},
		        {ECityPaletteAction::Music50, TEXT("MUSIC50%"), TEXT("Medium music volume")},
		        {ECityPaletteAction::Music75, TEXT("MUSIC75%"), TEXT("Louder music volume")},
		        {ECityPaletteAction::Music100, TEXT("MUSIC100%"), TEXT("Full music volume")},
		        {ECityPaletteAction::Back, TEXT("BACK"), TEXT("Return to game settings")}};
	case ECityPalettePage::Graphics:
		return {{ECityPaletteAction::GraphicsMode, TEXT("MODE"), TEXT("Fullscreen, borderless or windowed")},
		        {ECityPaletteAction::GraphicsPreviousResolution, TEXT("< RESOLUTION"), TEXT("Previous display size")},
		        {ECityPaletteAction::GraphicsNextResolution, TEXT("RESOLUTION >"), TEXT("Next display size")},
		        {ECityPaletteAction::GraphicsVSync, TEXT("VSYNC"), TEXT("Synchronize frames to the display")},
		        {ECityPaletteAction::GraphicsRenderScale, TEXT("SCENE SCALE"),
		         TEXT("50/75/100/125 percent; 100 is native detail, 125 supersamples; UI stays crisp")},
		        {ECityPaletteAction::GraphicsAntiAliasing, TEXT("ANTI-ALIASING"),
		         TEXT("2x/4x/8x MSAA edge smoothing; higher costs more GPU time")},
		        {ECityPaletteAction::GraphicsApply, TEXT("APPLY"), TEXT("Try this mode with a 15-second revert timer")},
		        {ECityPaletteAction::GraphicsKeep, TEXT("KEEP"), TEXT("Confirm and save this mode")},
		        {ECityPaletteAction::GraphicsRevert, TEXT("REVERT"), TEXT("Return to the previous confirmed mode")},
		        {ECityPaletteAction::Back, TEXT("BACK"), TEXT("Discard edits and return to settings")}};
	case ECityPalettePage::Settings:
		return {
            {ECityPaletteAction::ToggleTutorial, TEXT("TUTORIAL ON/OFF"), TEXT("Disable or resume contextual tutorial attention")},
            {ECityPaletteAction::ResetTutorial, TEXT("RESET TUTORIAL"), TEXT("Resume opening guidance; city and objective history are preserved")},
		    {ECityPaletteAction::OpenAudio, TEXT("AUDIO"),
		     TEXT("Effects, ambience and music volume and mute")},
		    {ECityPaletteAction::OpenDisasters, TEXT("DISASTERS"),
		     TEXT("Per-city random incident frequency; earthquake and tornado frequency; ordinary fires "
		          "continue")},
		    {ECityPaletteAction::OpenMainMenu, TEXT("MAIN MENU"),
		     TEXT("Continue, load review and new-city setup")},
		    {ECityPaletteAction::OpenLighting, TEXT("LIGHTING"),
		     TEXT("Automatic day/night or a fixed lighting preset")},
		    {ECityPaletteAction::CycleAnchor, TEXT("MENU POSITION"),
		     TEXT("Compare view-following, stable-on-open, and hand menu positions")},
		    {ECityPaletteAction::ToggleDebugInfo, TEXT("DEBUG MODE"),
		     TEXT("Show diagnostic text and unlock Debug Tools")},
		    {ECityPaletteAction::OpenSafetyTests, TEXT("DEBUG TOOLS"),
		     TEXT("Audio preview and real safety/disaster test events")},
		    {ECityPaletteAction::ToggleConditionIndicators, TEXT("MAP BADGES"),
		     TEXT("Show current building issues; select with Inspect for causes")},
		    {ECityPaletteAction::FocusNextCondition, TEXT("NEXT ISSUE"),
		     TEXT("Inspect the next affected lot, including issues outside the marker pool")},
		    {ECityPaletteAction::Save, TEXT("v_"), TEXT("Save city"), true},
		    {ECityPaletteAction::ReviewLoad, TEXT("LOAD"), TEXT("Review before loading the last saved city")},
		    {ECityPaletteAction::Reset, TEXT("@"), TEXT("Reset city; select again to confirm"), true},
		    {ECityPaletteAction::Back, TEXT("<"), TEXT("Back to main tools"), true}};
	case ECityPalettePage::SafetyTests:
		return {{ECityPaletteAction::SelectHome, TEXT("BUILD HOME"),
		         TEXT("Debug placement; normally homes grow in residential zones")},
		        {ECityPaletteAction::SelectShop, TEXT("BUILD SHOP"),
		         TEXT("Debug placement; normally shops grow in commercial zones")},
		        {ECityPaletteAction::PreviewAlarm, TEXT("TEST ALARM"),
		         TEXT("Hear the five-second alarm in the city; available while paused")},

		        {ECityPaletteAction::TestEarthquake, TEXT("TEST QUAKE"),
		         TEXT("Start near the first completed home/shop/factory; real repairable damage")},
		        {ECityPaletteAction::TestTornado, TEXT("TEST TORNADO"),
		         TEXT("Start a wandering tornado; real repairable damage")},
		        {ECityPaletteAction::SelectTestSickness, TEXT("MILD SICKNESS"),
		         TEXT("Point at a populated home/workplace and trigger; real hospital treatment")},
		        {ECityPaletteAction::SelectTestWaste, TEXT("PILE WASTE"),
		         TEXT("Point at a home/shop/factory and trigger to fill its waste cap")},
		        {ECityPaletteAction::SelectTestSevereSickness, TEXT("SEVERE SICKNESS"),
		         TEXT("Point at a populated home/workplace and trigger; requires an ambulance")},
		        {ECityPaletteAction::SelectTestFire, TEXT("START FIRE"),
		         TEXT("Select, close menu, point at a completed building and trigger; real damage/spread")},
		        {ECityPaletteAction::SelectTestCrime, TEXT("START CRIME"),
		         TEXT("Select, close menu, point at a home/shop/industry and trigger")},
		        {ECityPaletteAction::SelectTestArson, TEXT("START ARSON"),
		         TEXT("Trigger a fire and crime together; real damage, repair and police/fire response")},
		        {ECityPaletteAction::Back, TEXT("<"), TEXT("Back to game settings"), true}};
	case ECityPalettePage::Lighting:
		return {
		    {ECityPaletteAction::LightingDay, TEXT("DAY"), TEXT("Daylight presentation; does not change city time")},
		    {ECityPaletteAction::LightingDusk, TEXT("DUSK"), TEXT("Warm dusk presentation; does not change city time")},
		    {ECityPaletteAction::LightingNight, TEXT("NIGHT"),
		     TEXT("Cool night presentation with readable ambient light")},
		    {ECityPaletteAction::LightingAutomatic, TEXT("AUTO LIGHT"),
		     TEXT("Smooth day/night over one calendar day; follows speed and pause")},
		    {ECityPaletteAction::Back, TEXT("<"), TEXT("Back to game settings"), true}};
	case ECityPalettePage::Overlays:
		return {{ECityPaletteAction::OverlayHappiness, TEXT("HAPPINESS"),
		         TEXT("Home happiness; exact concerns and search status")},
		        {ECityPaletteAction::OverlayService, TEXT("WATER"), TEXT("Show water reach and capacity")},
		        {ECityPaletteAction::OverlayPower, TEXT("POWER"), TEXT("Show power status and capacity")},
		        {ECityPaletteAction::OverlayResidential, TEXT("RES DEMAND"), TEXT("Show residential demand")},
		        {ECityPaletteAction::OverlayCommercial, TEXT("JOB DEMAND"), TEXT("Show commercial and job demand")},
		        {ECityPaletteAction::OverlayIndustrial, TEXT("IND DEMAND"),
		         TEXT("Show industrial demand and viable parcels")},
		        {ECityPaletteAction::OverlayTraffic, TEXT("TRAFFIC"),
		         TEXT("Show motor-lane congestion; point for exact load")},
		        {ECityPaletteAction::OverlayPollution, TEXT("POLLUTION"),
		         TEXT("Show pollution spread and residential pressure")},
		        {ECityPaletteAction::OverlayWaste, TEXT("WASTE"),
		         TEXT("Lot accumulation/caps; inspect for planned collection")},
		        {ECityPaletteAction::OverlayClear, TEXT("CLEAR"), TEXT("Clear diagnostic overlay")},
		        {ECityPaletteAction::Back, TEXT("<"), TEXT("Back to main tools"), true}};
	case ECityPalettePage::Root:
	default:
		return {{ECityPaletteAction::SelectInspect, TEXT("INSPECT"), TEXT("Inspect a cell or building")},
		        {ECityPaletteAction::SelectRepair, TEXT("REPAIR"),
		         TEXT("Point at a damaged building to see its repair cost")},
		        {ECityPaletteAction::OpenBuild, TEXT("BUILD"), TEXT("Roads, buildings, and utilities")},
		        {ECityPaletteAction::OpenZone, TEXT("ZONE"), TEXT("Residential, commercial, and industrial zoning")},
		        {ECityPaletteAction::OpenCity, TEXT("CITY"), TEXT("City policies and taxes")},
		        {ECityPaletteAction::OpenOverlays, TEXT("OVERLAYS"), TEXT("City diagnostic overlays")},
		        {ECityPaletteAction::SelectDemolish, TEXT("DEMOLISH"), TEXT("Remove a road or building")},
		        {ECityPaletteAction::OpenSettings, TEXT("SETTINGS"),
		         TEXT("Game settings: lighting, menu position, save, load, and reset"), true}};
	}
}

const TCHAR* FCityPaletteModel::PageTitle(const ECityPalettePage Page)
{
	switch (Page)
	{
	case ECityPalettePage::MainMenu:
		return TEXT("CITY SIM");
	case ECityPalettePage::NewCitySetup:
		return TEXT("NEW CITY / SETUP");
	case ECityPalettePage::NewCityReview:
		return TEXT("NEW CITY / CONFIRM");
	case ECityPalettePage::LoadReview:
		return TEXT("LOAD CITY / REVIEW");
	case ECityPalettePage::Build:
		return TEXT("BUILD");
	case ECityPalettePage::WasteFacilities:
		return TEXT("WASTE SERVICES");
    case ECityPalettePage::CommercialAi:
		return TEXT("COMMERCIAL AI PILOT");
	case ECityPalettePage::TechnologyReview:
		return TEXT("TECHNOLOGY PROGRESSION");
	case ECityPalettePage::TechnologyFacilities:
		return TEXT("TECHNOLOGY");
	case ECityPalettePage::EducationFacilities:
		return TEXT("EDUCATION");
	case ECityPalettePage::PowerFacilities:
		return TEXT("POWER FACILITIES");
	case ECityPalettePage::WaterFacilities:
		return TEXT("WATER FACILITIES");
	case ECityPalettePage::Parks:
		return TEXT("PARKS / APPEAL");
	case ECityPalettePage::Roads:
		return TEXT("ROADS / UPGRADES");
	case ECityPalettePage::Zone:
		return TEXT("ZONE");
	case ECityPalettePage::City:
		return TEXT("CITY");
	case ECityPalettePage::Taxes:
		return TEXT("TAX POLICY");
	case ECityPalettePage::ArchiveReview:
		return TEXT("ARCHIVE SAVED CITY?");
	case ECityPalettePage::DisasterHistory:
		return TEXT("DISASTER EVENT HISTORY");
	case ECityPalettePage::Reports:
		return TEXT("CITY REPORTS");
	case ECityPalettePage::HousingHistory:
		return TEXT("RECENT HOUSING EVENTS");
	case ECityPalettePage::Policies:
		return TEXT("FUNDED CITY POLICIES");
	case ECityPalettePage::PolicyReview:
		return TEXT("REVIEW POLICY FUNDING");
	case ECityPalettePage::Loans:
		return TEXT("CITY LOANS");
	case ECityPalettePage::LoanReview:
		return TEXT("REVIEW LOAN TERMS");
	case ECityPalettePage::QuarterlyNews:
		return TEXT("QUARTERLY CITY NEWS");
	case ECityPalettePage::Disasters:
		return TEXT("NATURAL DISASTERS");
	case ECityPalettePage::Settings:
		return TEXT("GAME SETTINGS");
	case ECityPalettePage::Graphics:
		return TEXT("GRAPHICS / DISPLAY");
	case ECityPalettePage::Audio:
		return TEXT("AUDIO MIX");
	case ECityPalettePage::Lighting:
		return TEXT("LIGHTING");
	case ECityPalettePage::SafetyTests:
		return TEXT("DEBUG TOOLS");
	case ECityPalettePage::Overlays:
		return TEXT("OVERLAYS");
	default:
		return TEXT("CITY TOOLS");
	}
}

bool FCityPaletteModel::IsNavigationAction(const ECityPaletteAction Action)
{
	return Action == ECityPaletteAction::OpenCommercialAi || Action == ECityPaletteAction::OpenMainMenu || Action == ECityPaletteAction::ContinueCity ||
	       Action == ECityPaletteAction::OpenNewCitySetup || Action == ECityPaletteAction::ReviewLoad ||
	       Action == ECityPaletteAction::OpenTechnology || Action == ECityPaletteAction::OpenTechnologyFacilities ||
	       Action == ECityPaletteAction::OpenWasteFacilities || Action == ECityPaletteAction::OpenEducationFacilities ||
	       Action == ECityPaletteAction::OpenPowerFacilities || Action == ECityPaletteAction::OpenWaterFacilities ||
	       Action == ECityPaletteAction::OpenParks || Action == ECityPaletteAction::OpenBuild ||
	       Action == ECityPaletteAction::OpenRoads || Action == ECityPaletteAction::OpenZone ||
	       Action == ECityPaletteAction::OpenPolicies || Action == ECityPaletteAction::ReviewReservePolicy ||
	       Action == ECityPaletteAction::ReviewBursaryPolicy || Action == ECityPaletteAction::OpenLoans ||
	       Action == ECityPaletteAction::ReviewLoanSmall || Action == ECityPaletteAction::ReviewLoanMedium ||
	       Action == ECityPaletteAction::ReviewLoanLarge || Action == ECityPaletteAction::ReviewLoanPayoff ||
	       Action == ECityPaletteAction::OpenQuarterlyNews || Action == ECityPaletteAction::NewsMoney ||
	       Action == ECityPaletteAction::OpenHousingHistory || Action == ECityPaletteAction::ReportWaste ||
	       Action == ECityPaletteAction::OpenReports || Action == ECityPaletteAction::OpenCity ||
	       Action == ECityPaletteAction::OpenTaxes || Action == ECityPaletteAction::ReportDisasters ||
	       Action == ECityPaletteAction::OpenDisasters || Action == ECityPaletteAction::OpenSafetyTests ||
	       Action == ECityPaletteAction::OpenSettings || Action == ECityPaletteAction::OpenGraphics ||
	       Action == ECityPaletteAction::OpenAudio || Action == ECityPaletteAction::OpenLighting ||
	       Action == ECityPaletteAction::OpenOverlays || Action == ECityPaletteAction::Back ||
	       Action == ECityPaletteAction::Close;
}

TArray<FCityPaletteEntry> FCityPaletteModel::SurfaceEntries(const ECityPalettePage Page, const bool bHandPalette)
{
	if ((bHandPalette && Page != ECityPalettePage::CommercialAi && Page != ECityPalettePage::TechnologyReview && Page != ECityPalettePage::Reports &&
	     Page != ECityPalettePage::HousingHistory && Page != ECityPalettePage::QuarterlyNews &&
	     Page != ECityPalettePage::Loans && Page != ECityPalettePage::LoanReview &&
	     Page != ECityPalettePage::Policies && Page != ECityPalettePage::PolicyReview) ||
	    IsEntryPage(Page))
		return Entries(Page);
	TArray<FCityPaletteEntry> Result = {
	    {ECityPaletteAction::SelectInspect, TEXT("INSPECT"), TEXT("Inspect a cell or building")},
	    {ECityPaletteAction::OpenBuild, TEXT("BUILD"), TEXT("Roads, buildings, and utilities")},
	    {ECityPaletteAction::OpenZone, TEXT("ZONE"), TEXT("Residential, commercial, and industrial zoning")},
	    {ECityPaletteAction::OpenOverlays, TEXT("OVERLAYS"), TEXT("City diagnostic overlays")},
	    {ECityPaletteAction::OpenCity, TEXT("CITY"), TEXT("City policies and taxes")},
	    {ECityPaletteAction::SelectDemolish, TEXT("DEMOLISH"), TEXT("Remove a road or building")},
	    {ECityPaletteAction::OpenSettings, TEXT("SETTINGS"),
	     TEXT("Game settings: lighting, menu position, save, load, and reset"), true},
	    {ECityPaletteAction::SelectRepair, TEXT("REPAIR"), TEXT("Point at a damaged building to see its repair cost")}};
	if (Page != ECityPalettePage::Root)
		Result.Append(Entries(Page));
	return Result;
}

FCityPaletteRect FCityPaletteModel::ButtonRect(const ECityPalettePage Page, const int32 Index,
                                               const ECityPaletteAction Action, const bool bHandPalette)
{
	if(!bHandPalette && Page==ECityPalettePage::City && Action==ECityPaletteAction::OpenTaxes)
		return {FVector(1,-42,5.5),FVector2D(5.6,2.2)};
	if (!bHandPalette && Page == ECityPalettePage::City && Action == ECityPaletteAction::Back)
		return {FVector(1, 42, 5.5), FVector2D(5.6, 2.2)};
	if ((Page == ECityPalettePage::Reports || Page == ECityPalettePage::DisasterHistory ||
	     (Page == ECityPalettePage::HousingHistory || Page == ECityPalettePage::QuarterlyNews ||
	      Page == ECityPalettePage::Loans || Page == ECityPalettePage::LoanReview ||
	      Page == ECityPalettePage::Policies ||
	      (Page == ECityPalettePage::CommercialAi || Page == ECityPalettePage::TechnologyReview || Page == ECityPalettePage::PolicyReview))) &&
	    Index >= BarEntryCount)
	{
		// Range choices and navigation each occupy their own row above the toolbar.
		if (Page == ECityPalettePage::Reports)
		{
			int32 N = Index - BarEntryCount;
			if (Action == ECityPaletteAction::Back) N = 11;
			else if (Action == ECityPaletteAction::ReportMetricPrevious) N = 12;
			else if (Action == ECityPaletteAction::ReportMetricNext) N = 13;
			else if (Action == ECityPaletteAction::ReportPeriodPrevious) N = 14;
			else if (Action == ECityPaletteAction::ReportPeriodNext) N = 15;
			return {FVector(1, 27.0 - (N % 4) * 18.0, 22.0 - (N / 4) * 5.5), FVector2D(8.2, 2.2)};
		}
		const int32 N = Index - BarEntryCount;
		return {FVector(1, 24.0 - (N % 3) * 24.0, 22.0 - (N / 3) * 5.5), FVector2D(10.8, 2.2)};
	}
	if (IsEntryPage(Page))
	{
		const double Column = bHandPalette ? 16.0 : 22.0;
		return {FVector(1.0, Column - (Index % 3) * Column,
		                ((Page == ECityPalettePage::MainMenu) ? 0.0 : -8.0) - (Index / 3) * 6.0),
		        FVector2D(bHandPalette ? 7.3 : 10.0, 2.2)};
	}
	if ((!bHandPalette || Page == ECityPalettePage::Reports ||
	     (Page == ECityPalettePage::HousingHistory || Page == ECityPalettePage::QuarterlyNews ||
	      Page == ECityPalettePage::Loans || Page == ECityPalettePage::LoanReview ||
	      Page == ECityPalettePage::Policies ||
	      (Page == ECityPalettePage::CommercialAi || Page == ECityPalettePage::TechnologyReview || Page == ECityPalettePage::PolicyReview))) &&
	    Index < BarEntryCount)
	{
		// Append the gear without moving or shrinking the six accepted tool targets.
		if (Action == ECityPaletteAction::OpenSettings)
			return {FVector(1.0, -42.0, 0.0), FVector2D(4.4, 2.0)};
		if (Action == ECityPaletteAction::SelectRepair)
			return {FVector(1.0, 42.0, 0.0), FVector2D(5.6, 2.0)};
		return {FVector(1.0, 30.0 - Index * 12.0, 0.0), FVector2D(5.6, 2.0)};
	}
	if (Page == ECityPalettePage::Taxes)
	{
		const double Column = 18.0;
		const double Top = bHandPalette ? 11.0 : 23.0;
		const double Spacing = bHandPalette ? 5.0 : 5.5;
		double Y = Column;
		int32 Row = 0;
		switch (Action)
		{
		case ECityPaletteAction::TaxResidentialTrack:
			break;
		case ECityPaletteAction::TaxCommercialTrack:
			Y = 0;
			break;
		case ECityPaletteAction::TaxIndustrialTrack:
			Y = -Column;
			break;
		case ECityPaletteAction::TaxResidentialDown:
			Row = 1;
			break;
		case ECityPaletteAction::TaxCommercialDown:
			Y = 0;
			Row = 1;
			break;
		case ECityPaletteAction::TaxIndustrialDown:
			Y = -Column;
			Row = 1;
			break;
		case ECityPaletteAction::TaxResidentialUp:
			Row = 2;
			break;
		case ECityPaletteAction::TaxCommercialUp:
			Y = 0;
			Row = 2;
			break;
		case ECityPaletteAction::TaxIndustrialUp:
			Y = -Column;
			Row = 2;
			break;
		case ECityPaletteAction::ApplyTaxes:
			Row = 3;
			break;
		case ECityPaletteAction::CancelTaxes:
			Y = -Column;
			Row = 3;
			break;
		default:
			break;
		}
		return {FVector(1.0, Y, Top - Row * Spacing), FVector2D(7.0, 1.8)};
	}
	if (bHandPalette)
	{
		if (Page == ECityPalettePage::Audio)
		{
			const int32 Column = Index / 5;
			return {FVector(1.0, Action == ECityPaletteAction::Back ? 0.0 : 18.0 - Column * 18.0,
			                Action == ECityPaletteAction::Back ? -14.0 : 11.0 - (Index % 5) * 5.0),
			        FVector2D(8.0, 1.8)};
		}
		const int32 Rows = FMath::Max(5, FMath::DivideAndRoundUp(Entries(Page).Num(), 2));
		return {FVector(1.0, Index >= Rows ? 8.0 : -8.0, 11.0 - (Index % Rows) * 5.0), FVector2D(7.0, 1.8)};
	}
	const int32 DrawerIndex = Index - BarEntryCount;
	if (Page == ECityPalettePage::Audio)
		return {FVector(1.0, Action == ECityPaletteAction::Back ? 0.0 : 24.0 - (DrawerIndex / 5) * 24.0,
		                Action == ECityPaletteAction::Back ? -5.5 : 28.0 - (DrawerIndex % 5) * 5.5),
		        FVector2D(10.8, 2.2)};
	// City and Settings need five rows; keep the last row above the persistent toolbar.
	const double DrawerTop = Page == ECityPalettePage::City || Page == ECityPalettePage::Settings ? 27.5 : 22.0;
	return {FVector(1.0, 24.0 - (DrawerIndex % 3) * 24.0, DrawerTop - (DrawerIndex / 3) * 5.5), FVector2D(10.8, 2.2)};
}

FCityPaletteRect FCityPaletteModel::SurfaceRect(const ECityPalettePage Page, const bool bHandPalette)
{
	if (Page == ECityPalettePage::QuarterlyNews || Page == ECityPalettePage::Loans ||
	    Page == ECityPalettePage::LoanReview || Page == ECityPalettePage::Policies ||
	    (Page == ECityPalettePage::CommercialAi || Page == ECityPalettePage::TechnologyReview || Page == ECityPalettePage::PolicyReview))
		return {FVector(0,0,38),FVector2D(49,47)};
	if ((Page == ECityPalettePage::Reports || Page == ECityPalettePage::DisasterHistory ||
	     (Page == ECityPalettePage::HousingHistory || Page == ECityPalettePage::QuarterlyNews ||
	      Page == ECityPalettePage::Loans || Page == ECityPalettePage::LoanReview ||
	      Page == ECityPalettePage::Policies ||
	      (Page == ECityPalettePage::CommercialAi || Page == ECityPalettePage::TechnologyReview || Page == ECityPalettePage::PolicyReview))))
		return {FVector(0, 0, 23), FVector2D(49, 32)};
	if (IsEntryPage(Page))
		return Page == ECityPalettePage::MainMenu
		           ? FCityPaletteRect{FVector(0, 0, 11), FVector2D(bHandPalette ? 25.0 : 34.0, 22.0)}
		           : FCityPaletteRect{FVector(0, 0, 8), FVector2D(34.0, 34.0)};
	if (bHandPalette && (Page == ECityPalettePage::City || Page == ECityPalettePage::Settings))
	{
		const int32 Rows = FMath::Max(5, FMath::DivideAndRoundUp(Entries(Page).Num(), 2));
		const double Bottom = FMath::Min(-21.0, 11.0 - (Rows - 1) * 5.0 - 2.0);
		return {FVector(0, 0, (18.0 + Bottom) / 2.0), FVector2D(17, (18.0 - Bottom) / 2.0)};
	}
	if (bHandPalette)
		return {FVector(0.0, 0.0, -1.5),
		        FVector2D(Page == ECityPalettePage::Taxes || Page == ECityPalettePage::Audio ? 27.0 : 17.0, 19.5)};
	const double Top = Page == ECityPalettePage::Root ? 8.0 : 31.0;
	return {FVector(0.0, 0.0, (Top - 9.0) / 2.0), FVector2D(49.0, (Top + 9.0) / 2.0)};
}

ECityPalettePage FCityPaletteModel::ParentPage(const ECityPalettePage Page)
{
    if (Page == ECityPalettePage::CommercialAi) return ECityPalettePage::TechnologyReview;
	if(Page == ECityPalettePage::PolicyReview) return ECityPalettePage::Policies;
	if (Page == ECityPalettePage::LoanReview)
		return ECityPalettePage::Loans;
	if (Page == ECityPalettePage::ArchiveReview)
		return ECityPalettePage::LoadReview;
	if (Page == ECityPalettePage::MainMenu)
		return ECityPalettePage::MainMenu;
	if (Page == ECityPalettePage::NewCityReview)
		return ECityPalettePage::NewCitySetup;
	if (Page == ECityPalettePage::NewCitySetup || Page == ECityPalettePage::LoadReview)
		return ECityPalettePage::MainMenu;
	if (Page == ECityPalettePage::TechnologyFacilities || Page == ECityPalettePage::WasteFacilities ||
	    Page == ECityPalettePage::EducationFacilities || Page == ECityPalettePage::PowerFacilities ||
	    Page == ECityPalettePage::WaterFacilities || Page == ECityPalettePage::Roads ||
	    Page == ECityPalettePage::Parks)
		return ECityPalettePage::Build;
	if (Page == ECityPalettePage::Taxes || Page == ECityPalettePage::Reports ||
	    (Page == ECityPalettePage::HousingHistory || Page == ECityPalettePage::QuarterlyNews ||
	     Page == ECityPalettePage::Loans || Page == ECityPalettePage::LoanReview ||
	     Page == ECityPalettePage::Policies ||
	     (Page == ECityPalettePage::CommercialAi || Page == ECityPalettePage::TechnologyReview || Page == ECityPalettePage::PolicyReview)))
		return ECityPalettePage::City;
	if (Page == ECityPalettePage::DisasterHistory)
		return ECityPalettePage::Disasters;
	if (Page == ECityPalettePage::Graphics || Page == ECityPalettePage::Audio || Page == ECityPalettePage::Disasters ||
	    Page == ECityPalettePage::Lighting || Page == ECityPalettePage::SafetyTests)
		return ECityPalettePage::Settings;
	return ECityPalettePage::Root;
}

bool FCityPaletteModel::IsEntryPage(const ECityPalettePage Page)
{
	return Page == ECityPalettePage::ArchiveReview || Page == ECityPalettePage::MainMenu ||
	       Page == ECityPalettePage::NewCitySetup || Page == ECityPalettePage::LoadReview ||
	       Page == ECityPalettePage::NewCityReview;
}

FName FCityPaletteModel::ConstructionDefinition(ECityPaletteAction Action)
{
	switch (Action)
	{
	case ECityPaletteAction::SelectHome:
		return TEXT("Home");
	case ECityPaletteAction::SelectShop:
		return TEXT("Shop");
	case ECityPaletteAction::SelectPower:
		return TEXT("Power");
	case ECityPaletteAction::SelectService:
		return TEXT("Service");
	case ECityPaletteAction::SelectSchool:
		return TEXT("School");
	case ECityPaletteAction::SelectPrimarySchool:
		return TEXT("PrimarySchool");
	case ECityPaletteAction::SelectSecondarySchool:
		return TEXT("SecondarySchool");
	case ECityPaletteAction::SelectDigitalExchange:
		return TEXT("DigitalExchange");
	case ECityPaletteAction::SelectComputingCenter:
		return TEXT("ComputingCenter");
    case ECityPaletteAction::SelectAIInstitute: return TEXT("AIInstitute");
    case ECityPaletteAction::SelectEnterpriseAICenter: return TEXT("EnterpriseAICenter");
	case ECityPaletteAction::SelectResearchCampus:
		return TEXT("ResearchCampus");
	case ECityPaletteAction::SelectCollege:
		return TEXT("College");
	case ECityPaletteAction::SelectPolice:
		return TEXT("Police");
	case ECityPaletteAction::SelectFire:
		return TEXT("Fire");
	case ECityPaletteAction::SelectWasteDepot:
		return TEXT("WasteDepot");
	case ECityPaletteAction::SelectRecycling:
		return TEXT("Recycling");
	case ECityPaletteAction::SelectHospital:
		return TEXT("Hospital");
	case ECityPaletteAction::SelectPark:
		return TEXT("Park");
	case ECityPaletteAction::SelectNeighborhoodPark:
		return TEXT("NeighborhoodPark");
	case ECityPaletteAction::SelectBasketballPark:
		return TEXT("BasketballPark");
	case ECityPaletteAction::SelectSkatePark:
		return TEXT("SkatePark");
	case ECityPaletteAction::SelectBaseballPark:
		return TEXT("BaseballPark");
	case ECityPaletteAction::SelectSoccerPark:
		return TEXT("SoccerPark");
	case ECityPaletteAction::SelectCityPark:
		return TEXT("CityPark");
	case ECityPaletteAction::SelectWaterTower:
		return TEXT("WaterTower");
	case ECityPaletteAction::SelectCoalPower:
		return TEXT("CoalPower");
	case ECityPaletteAction::SelectOilPower:
		return TEXT("OilPower");
	case ECityPaletteAction::SelectWindPower:
		return TEXT("WindPower");
	case ECityPaletteAction::SelectSolarPower:
		return TEXT("SolarPower");
	case ECityPaletteAction::SelectNuclearPower:
		return TEXT("NuclearPower");
	case ECityPaletteAction::SelectDirtRoad:
		return TEXT("DirtRoad");
	case ECityPaletteAction::SelectLocalRoad:
		return TEXT("LocalRoad");
	case ECityPaletteAction::SelectAvenue:
		return TEXT("Avenue");
	case ECityPaletteAction::SelectHighway:
		return TEXT("Highway");
	default:
		return NAME_None;
	}
}
