#include "builtin_datasets.hpp"

namespace oceandl {

namespace {

constexpr std::string_view kOisstHighresPath = "Datasets/noaa.oisst.v2.highres";

}  // namespace

const std::vector<BuiltinDatasetSpec>& builtin_dataset_specs() {
    static const std::vector<BuiltinDatasetSpec> specs{
        {
            .id = "oisst",
            .display_name = "NOAA OISST Daily SST Mean",
            .description =
                "Daily mean sea surface temperature NetCDF files from NOAA Optimum "
                "Interpolation SST V2.1.",
            .provider_key = "psl",
            .default_path = kOisstHighresPath,
            .filename_pattern = "sst.day.mean.{year}.nc",
            .file_mode = FileMode::PerYear,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = 1981,
            .end_year = std::nullopt,
        },
        {
            .id = "oisst_icec_day_mean",
            .display_name = "NOAA OISST Daily Sea Ice Concentration",
            .description =
                "Daily mean sea ice concentration NetCDF files from the NOAA OISST "
                "high-resolution archive.",
            .provider_key = "psl",
            .default_path = kOisstHighresPath,
            .filename_pattern = "icec.day.mean.{year}.nc",
            .file_mode = FileMode::PerYear,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = 1981,
            .end_year = std::nullopt,
        },
        {
            .id = "oisst_sst_day_anom",
            .display_name = "NOAA OISST Daily SST Anomaly",
            .description =
                "Daily sea surface temperature anomaly NetCDF files from the NOAA "
                "OISST high-resolution archive.",
            .provider_key = "psl",
            .default_path = kOisstHighresPath,
            .filename_pattern = "sst.day.anom.{year}.nc",
            .file_mode = FileMode::PerYear,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = 1981,
            .end_year = std::nullopt,
        },
        {
            .id = "oisst_sst_day_err",
            .display_name = "NOAA OISST Daily SST Error",
            .description =
                "Daily sea surface temperature error NetCDF files from the NOAA "
                "OISST high-resolution archive.",
            .provider_key = "psl",
            .default_path = kOisstHighresPath,
            .filename_pattern = "sst.day.err.{year}.nc",
            .file_mode = FileMode::PerYear,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = 1981,
            .end_year = std::nullopt,
        },
        {
            .id = "oisst_icec_day_mean_ltm_1991_2020",
            .display_name = "NOAA OISST Daily Ice Concentration LTM 1991-2020",
            .description =
                "Daily long-term mean sea ice concentration for 1991-2020 from the "
                "NOAA OISST high-resolution archive.",
            .provider_key = "psl",
            .default_path = kOisstHighresPath,
            .filename_pattern = "icec.day.mean.ltm.1991-2020.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "oisst_icec_mon_ltm_1991_2020",
            .display_name = "NOAA OISST Monthly Ice Concentration LTM 1991-2020",
            .description =
                "Monthly long-term mean sea ice concentration for 1991-2020 from "
                "the NOAA OISST high-resolution archive.",
            .provider_key = "psl",
            .default_path = kOisstHighresPath,
            .filename_pattern = "icec.mon.ltm.1991-2020.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "oisst_icec_mon_mean",
            .display_name = "NOAA OISST Monthly Ice Concentration",
            .description =
                "Monthly mean sea ice concentration NetCDF file from the NOAA "
                "OISST high-resolution archive.",
            .provider_key = "psl",
            .default_path = kOisstHighresPath,
            .filename_pattern = "icec.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "oisst_icec_week_mean",
            .display_name = "NOAA OISST Weekly Ice Concentration",
            .description =
                "Weekly mean sea ice concentration NetCDF file from the NOAA "
                "OISST high-resolution archive.",
            .provider_key = "psl",
            .default_path = kOisstHighresPath,
            .filename_pattern = "icec.week.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "oisst_lsmask",
            .display_name = "NOAA OISST Land-Sea Mask",
            .description =
                "Land-sea mask NetCDF file from the NOAA OISST high-resolution archive.",
            .provider_key = "psl",
            .default_path = kOisstHighresPath,
            .filename_pattern = "lsmask.oisst.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "oisst_sst_day_mean_ltm_1971_2000",
            .display_name = "NOAA OISST Daily SST LTM 1971-2000",
            .description =
                "Daily long-term mean sea surface temperature for 1971-2000 from "
                "the NOAA OISST high-resolution archive.",
            .provider_key = "psl",
            .default_path = kOisstHighresPath,
            .filename_pattern = "sst.day.mean.ltm.1971-2000.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "oisst_sst_day_mean_ltm_1982_2010",
            .display_name = "NOAA OISST Daily SST LTM 1982-2010",
            .description =
                "Daily long-term mean sea surface temperature for 1982-2010 from "
                "the NOAA OISST high-resolution archive.",
            .provider_key = "psl",
            .default_path = kOisstHighresPath,
            .filename_pattern = "sst.day.mean.ltm.1982-2010.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "oisst_sst_day_mean_ltm_1991_2020",
            .display_name = "NOAA OISST Daily SST LTM 1991-2020",
            .description =
                "Daily long-term mean sea surface temperature for 1991-2020 from "
                "the NOAA OISST high-resolution archive.",
            .provider_key = "psl",
            .default_path = kOisstHighresPath,
            .filename_pattern = "sst.day.mean.ltm.1991-2020.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "oisst_sst_day_mean_ltm",
            .display_name = "NOAA OISST Daily SST LTM",
            .description =
                "Daily long-term mean sea surface temperature NetCDF file from the "
                "NOAA OISST high-resolution archive.",
            .provider_key = "psl",
            .default_path = kOisstHighresPath,
            .filename_pattern = "sst.day.mean.ltm.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "oisst_sst_mon_ltm_1991_2020",
            .display_name = "NOAA OISST Monthly SST LTM 1991-2020",
            .description =
                "Monthly long-term mean sea surface temperature for 1991-2020 from "
                "the NOAA OISST high-resolution archive.",
            .provider_key = "psl",
            .default_path = kOisstHighresPath,
            .filename_pattern = "sst.mon.ltm.1991-2020.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "oisst_sst_mon_mean",
            .display_name = "NOAA OISST Monthly SST",
            .description =
                "Monthly mean sea surface temperature NetCDF file from the NOAA "
                "OISST high-resolution archive.",
            .provider_key = "psl",
            .default_path = kOisstHighresPath,
            .filename_pattern = "sst.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "oisst_sst_week_mean",
            .display_name = "NOAA OISST Weekly SST",
            .description =
                "Weekly mean sea surface temperature NetCDF file from the NOAA "
                "OISST high-resolution archive.",
            .provider_key = "psl",
            .default_path = kOisstHighresPath,
            .filename_pattern = "sst.week.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "gpcp",
            .display_name = "GPCP Monthly Precipitation",
            .description =
                "Global monthly precipitation estimates from the Global Precipitation "
                "Climatology Project.",
            .provider_key = "psl",
            .default_path = "Datasets/gpcp",
            .filename_pattern = "precip.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "air",
            .display_name = "NCEP Reanalysis Air Temperature",
            .description =
                "Monthly mean near-surface air temperature from the NCEP/NCAR Reanalysis.",
            .provider_key = "psl",
            .default_path = "Datasets/ncep.reanalysis.derived/surface",
            .filename_pattern = "air.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "mslp",
            .display_name = "NCEP Reanalysis Mean Sea Level Pressure",
            .description =
                "Monthly mean sea level pressure from the NCEP/NCAR Reanalysis.",
            .provider_key = "psl",
            .default_path = "Datasets/ncep.reanalysis.derived/surface",
            .filename_pattern = "mslp.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "uwnd_surface",
            .display_name = "NCEP Reanalysis Surface Zonal Wind",
            .description =
                "Monthly mean near-surface zonal wind from the NCEP/NCAR Reanalysis.",
            .provider_key = "psl",
            .default_path = "Datasets/ncep.reanalysis.derived/surface",
            .filename_pattern = "uwnd.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "vwnd_surface",
            .display_name = "NCEP Reanalysis Surface Meridional Wind",
            .description =
                "Monthly mean near-surface meridional wind from the NCEP/NCAR Reanalysis.",
            .provider_key = "psl",
            .default_path = "Datasets/ncep.reanalysis.derived/surface",
            .filename_pattern = "vwnd.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "rhum_surface",
            .display_name = "NCEP Reanalysis Surface Relative Humidity",
            .description =
                "Monthly mean near-surface relative humidity from the NCEP/NCAR Reanalysis.",
            .provider_key = "psl",
            .default_path = "Datasets/ncep.reanalysis.derived/surface",
            .filename_pattern = "rhum.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "pr_wtr",
            .display_name = "NCEP Reanalysis Precipitable Water",
            .description =
                "Monthly mean precipitable water from the NCEP/NCAR Reanalysis.",
            .provider_key = "psl",
            .default_path = "Datasets/ncep.reanalysis.derived/surface",
            .filename_pattern = "pr_wtr.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "hgt_pressure",
            .display_name = "NCEP Reanalysis 2 Geopotential Height",
            .description =
                "Monthly mean geopotential height on pressure levels from the NCEP/DOE Reanalysis 2.",
            .provider_key = "psl",
            .default_path = "Datasets/ncep.reanalysis2.derived/pressure",
            .filename_pattern = "hgt.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "omega_pressure",
            .display_name = "NCEP Reanalysis 2 Vertical Velocity",
            .description =
                "Monthly mean vertical velocity on pressure levels from the NCEP/DOE Reanalysis 2.",
            .provider_key = "psl",
            .default_path = "Datasets/ncep.reanalysis2.derived/pressure",
            .filename_pattern = "omega.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
    };
    return specs;
}

std::map<std::string, std::string> builtin_provider_base_urls() {
    return {
        {"psl", "https://downloads.psl.noaa.gov"},
    };
}

}  // namespace oceandl
