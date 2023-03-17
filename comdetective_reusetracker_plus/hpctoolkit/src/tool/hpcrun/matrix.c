#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "matrix.h"
#include <limits.h>

int fs_matrix_size;
int ts_matrix_size;
int as_matrix_size;

int fs_core_matrix_size;
int ts_core_matrix_size;
int as_core_matrix_size;

int max_consecutive_count = 0;

int HASHTABLESIZE;

double fs_matrix[2000][2000];
double ts_matrix[2000][2000];
double as_matrix[2000][2000];
double invalidation_matrix[2000][2000];

double fs_core_matrix[2000][2000];
double ts_core_matrix[2000][2000];
double as_core_matrix[2000][2000];
double invalidation_core_matrix[2000][2000];


double war_fs_matrix[2000][2000];
double war_ts_matrix[2000][2000];
double war_as_matrix[2000][2000];

double war_fs_core_matrix[2000][2000];
double war_ts_core_matrix[2000][2000];
double war_as_core_matrix[2000][2000];

double waw_fs_matrix[2000][2000];
double waw_ts_matrix[2000][2000];
double waw_as_matrix[2000][2000];

double waw_fs_core_matrix[2000][2000];
double waw_ts_core_matrix[2000][2000];
double waw_as_core_matrix[2000][2000];

long number_of_traps;

long global_store_sampling_period;
long global_load_sampling_period;

extern char output_directory[PATH_MAX];

extern const char * hpcrun_files_executable_name();

// before
__thread long number_of_sample = 0;
__thread long number_of_load_sample = 0;
__thread long number_of_store_sample = 0;
__thread long number_of_load_store_sample = 0;
__thread long number_of_load_store_sample_all_loads = 0;
__thread long number_of_load_store_sample_all_stores = 0;
__thread long number_of_arming = 0;
__thread long number_of_caught_traps = 0;
__thread long number_of_caught_read_traps = 0;
__thread long number_of_caught_write_traps = 0;
__thread long number_of_caught_read_write_traps = 0;
__thread long number_of_bulletin_board_updates_before = 0;
__thread long number_of_bulletin_board_updates = 0;
__thread long number_of_residues = 0;
// after

int consecutive_access_count_array[50];

int consecutive_wasted_trap_array[50];

// comdetective stats begin
double fs_volume;
double fs_core_volume;
double ts_volume;
double ts_core_volume;
double as_volume;
double as_core_volume;
double invalidation_volume;
double invalidation_core_volume;
double cache_line_transfer;
double cache_line_transfer_millions;
double cache_line_transfer_gbytes;
double war_fs_volume;
double war_fs_core_volume;
double war_ts_volume;
double war_ts_core_volume;
double war_as_volume;
double war_as_core_volume;
double war_cache_line_transfer;
double war_cache_line_transfer_millions;
double war_cache_line_transfer_gbytes;
double waw_fs_volume;
double waw_fs_core_volume;
double waw_ts_volume;
double waw_ts_core_volume;
double waw_as_volume;
double waw_as_core_volume;
double waw_cache_line_transfer;
double waw_cache_line_transfer_millions;
double waw_cache_line_transfer_gbytes;

// comdetective stats end

void adjust_communication_volume(double scale_ratio) {
	//double scale_ratio = mem_access_sample / sample_count;
          fprintf(stderr, "scale_ratio: %0.2lf\n", scale_ratio);
	  printf("as_matrix_size: %d\n", as_matrix_size);
          for(int i = 0; i <= as_matrix_size; i++) {
                  for(int j = 0; j <= as_matrix_size; j++) {
                        as_matrix[i][j] = as_matrix[i][j] * scale_ratio;
                        fprintf(stderr, "%0.2lf ", as_matrix[i][j]);
                  }
                  fprintf(stderr, "\n");
          }
          for(int i = 0; i <= as_core_matrix_size; i++) {
                  for(int j = 0; j <= as_core_matrix_size; j++)
                        as_core_matrix[i][j] = as_core_matrix[i][j] * scale_ratio;
          }
          for(int i = 0; i <= fs_matrix_size; i++) {
                  for(int j = 0; j <= fs_matrix_size; j++)
                        fs_matrix[i][j] = fs_matrix[i][j] * scale_ratio;
          }
          for(int i = 0; i <= fs_core_matrix_size; i++) {
                  for(int j = 0; j <= fs_core_matrix_size; j++)
                        fs_core_matrix[i][j] = fs_core_matrix[i][j] * scale_ratio;
          }
          for(int i = 0; i <= ts_matrix_size; i++) {
                  for(int j = 0; j <= ts_matrix_size; j++)
                        ts_matrix[i][j] = ts_matrix[i][j] * scale_ratio;
          }
          for(int i = 0; i <= ts_core_matrix_size; i++) {
                  for(int j = 0; j <= ts_core_matrix_size; j++)
                        ts_core_matrix[i][j] = ts_core_matrix[i][j] * scale_ratio;
          }
}

	void 
dump_fs_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX]; 
	sprintf(file_name, "%s/%s-%ld-fs_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );
	fp = fopen (file_name, "w+");
	//printf("fs_matrix_size: %d\n", fs_matrix_size);
	double total= 0;
	for(int i = fs_matrix_size; i >= 0; i--) 
	{
		for (int j = 0; j <= fs_matrix_size; j++)
		{
			if(j < fs_matrix_size) {
				fprintf(fp, "%0.2lf,", fs_matrix[i][j] + fs_matrix[j][i]);
				total += fs_matrix[i][j];
				//printf("%0.2lf,", fs_matrix[i][j] + fs_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", fs_matrix[i][j] + fs_matrix[j][i]);
				total += fs_matrix[i][j];
				//printf("%0.2lf", fs_matrix[i][j] + fs_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	fclose(fp);
	fs_volume = total;
}

	void 
dump_fs_core_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX];
	sprintf(file_name, "%s/%s-%ld-fs_core_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );
	fp = fopen (file_name, "w+");
	//printf("fs_core_matrix_size: %d\n", fs_core_matrix_size);
	double total= 0;
	for(int i = fs_core_matrix_size; i >= 0; i--)
	{
		for (int j = 0; j <= fs_core_matrix_size; j++)
		{
			if(j < fs_core_matrix_size) {
				fprintf(fp, "%0.2lf,", fs_core_matrix[i][j] + fs_core_matrix[j][i]);
				total += fs_core_matrix[i][j];
				//printf("%0.2lf,", fs_core_matrix[i][j] + fs_core_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", fs_core_matrix[i][j] + fs_core_matrix[j][i]);
				total += fs_core_matrix[i][j];
				//printf("%0.2lf", fs_core_matrix[i][j] + fs_core_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	fclose(fp);
	fs_core_volume = total;
}

	void 
dump_ts_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX];
	sprintf(file_name, "%s/%s-%ld-ts_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );
	fp = fopen (file_name, "w+");
	//printf("ts_matrix_size: %d\n", ts_matrix_size);
	double total = 0;
	for(int i = ts_matrix_size; i >= 0; i--) 
	{
		for (int j = 0; j <= ts_matrix_size; j++)
		{
			if(j < ts_matrix_size) {
				fprintf(fp, "%0.2lf,", ts_matrix[i][j] + ts_matrix[j][i]);
				total += ts_matrix[i][j];
				//printf("%0.2lf,", ts_matrix[i][j] + ts_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", ts_matrix[i][j] + ts_matrix[j][i]);
				total += ts_matrix[i][j];
				//printf("%0.2lf", ts_matrix[i][j] + ts_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	fclose(fp);
	ts_volume = total;
}

	void 
dump_ts_core_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX];
	sprintf(file_name, "%s/%s-%ld-ts_core_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );
	fp = fopen (file_name, "w+");
	//printf("ts_core_matrix_size: %d\n", ts_core_matrix_size);
	double total = 0;
	for(int i = ts_core_matrix_size; i >= 0; i--)
	{
		for (int j = 0; j <= ts_core_matrix_size; j++)
		{
			if(j < ts_core_matrix_size) {
				fprintf(fp, "%0.2lf,", ts_core_matrix[i][j] + ts_core_matrix[j][i]);
				total += ts_core_matrix[i][j];
				//printf("%0.2lf,", ts_core_matrix[i][j] + ts_core_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", ts_core_matrix[i][j] + ts_core_matrix[j][i]);
				total += ts_core_matrix[i][j];
				//printf("%0.2lf", ts_core_matrix[i][j] + ts_core_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	fclose(fp);
	ts_core_volume = total;
}

	void 
dump_as_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX];
	long timeprint = (long) clock();
	sprintf(file_name, "%s/%s-%ld-as_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );

	fp = fopen (file_name, "w+");
	//printf("as_matrix_size: %d\n", as_matrix_size);
	double total = 0;
	//printf("all sharing matrix:\n");
	for(int i = as_matrix_size; i >= 0; i--)
	{
		for (int j = 0; j <= as_matrix_size; j++)
		{
			if(j < as_matrix_size) {
				fprintf(fp, "%0.2lf,", as_matrix[i][j] + as_matrix[j][i]);
				total += as_matrix[i][j];
				//printf("%0.2lf,", as_matrix[i][j] + as_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", as_matrix[i][j] + as_matrix[j][i]);
				total += as_matrix[i][j];
				//printf("%0.2lf", as_matrix[i][j] + as_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	as_volume = total;
	fclose(fp);
}

	void 
dump_as_core_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX];
	long timeprint = (long) clock();
	sprintf(file_name, "%s/%s-%ld-as_core_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );
	fp = fopen (file_name, "w+");
	//printf("as_matrix_size: %d\n", as_matrix_size);
	double total = 0;
	//printf("all sharing matrix:\n");
	for(int i = as_core_matrix_size; i >= 0; i--)
	{
		for (int j = 0; j <= as_core_matrix_size; j++)
		{
			if(j < as_core_matrix_size) {
				fprintf(fp, "%0.2lf,", as_core_matrix[i][j] + as_core_matrix[j][i]);
				total += as_core_matrix[i][j];
				//printf("%0.2lf,", as_core_matrix[i][j] + as_core_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", as_core_matrix[i][j] + as_core_matrix[j][i]);
				total += as_core_matrix[i][j];
				//printf("%0.2lf", as_core_matrix[i][j] + as_core_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	as_core_volume = total;
	cache_line_transfer = total;
	cache_line_transfer_millions = total/(1000000);
	cache_line_transfer_gbytes = total*64/(1024*1024*1024);
	fclose(fp);
}

	void 
dump_war_fs_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX]; 
	sprintf(file_name, "%s/%s-%ld-war_fs_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );
	fp = fopen (file_name, "w+");
	//printf("fs_matrix_size: %d\n", fs_matrix_size);
	double total= 0;
	for(int i = fs_matrix_size; i >= 0; i--) 
	{
		for (int j = 0; j <= fs_matrix_size; j++)
		{
			if(j < fs_matrix_size) {
				fprintf(fp, "%0.2lf,", war_fs_matrix[i][j] + war_fs_matrix[j][i]);
				total += war_fs_matrix[i][j];
				//printf("%0.2lf,", fs_matrix[i][j] + fs_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", war_fs_matrix[i][j] + war_fs_matrix[j][i]);
				total += war_fs_matrix[i][j];
				//printf("%0.2lf", fs_matrix[i][j] + fs_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	fclose(fp);
	war_fs_volume = total;
}

	void 
dump_war_fs_core_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX];
	sprintf(file_name, "%s/%s-%ld-war_fs_core_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );
	fp = fopen (file_name, "w+");
	//printf("fs_core_matrix_size: %d\n", fs_core_matrix_size);
	double total= 0;
	for(int i = fs_core_matrix_size; i >= 0; i--)
	{
		for (int j = 0; j <= fs_core_matrix_size; j++)
		{
			if(j < fs_core_matrix_size) {
				fprintf(fp, "%0.2lf,", war_fs_core_matrix[i][j] + war_fs_core_matrix[j][i]);
				total += war_fs_core_matrix[i][j];
				//printf("%0.2lf,", fs_core_matrix[i][j] + fs_core_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", war_fs_core_matrix[i][j] + war_fs_core_matrix[j][i]);
				total += war_fs_core_matrix[i][j];
				//printf("%0.2lf", fs_core_matrix[i][j] + fs_core_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	fclose(fp);
	war_fs_core_volume = total;
}

	void 
dump_war_ts_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX];
	sprintf(file_name, "%s/%s-%ld-war_ts_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );
	fp = fopen (file_name, "w+");
	//printf("ts_matrix_size: %d\n", ts_matrix_size);
	double total = 0;
	for(int i = ts_matrix_size; i >= 0; i--) 
	{
		for (int j = 0; j <= ts_matrix_size; j++)
		{
			if(j < ts_matrix_size) {
				fprintf(fp, "%0.2lf,", war_ts_matrix[i][j] + war_ts_matrix[j][i]);
				total += war_ts_matrix[i][j];
				//printf("%0.2lf,", ts_matrix[i][j] + ts_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", war_ts_matrix[i][j] + war_ts_matrix[j][i]);
				total += war_ts_matrix[i][j];
				//printf("%0.2lf", ts_matrix[i][j] + ts_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	fclose(fp);
	war_ts_volume = total;
}

	void 
dump_war_ts_core_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX];
	sprintf(file_name, "%s/%s-%ld-war_ts_core_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );
	fp = fopen (file_name, "w+");
	//printf("ts_core_matrix_size: %d\n", ts_core_matrix_size);
	double total = 0;
	for(int i = ts_core_matrix_size; i >= 0; i--)
	{
		for (int j = 0; j <= ts_core_matrix_size; j++)
		{
			if(j < ts_core_matrix_size) {
				fprintf(fp, "%0.2lf,", war_ts_core_matrix[i][j] + war_ts_core_matrix[j][i]);
				total += war_ts_core_matrix[i][j];
				//printf("%0.2lf,", ts_core_matrix[i][j] + ts_core_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", war_ts_core_matrix[i][j] + war_ts_core_matrix[j][i]);
				total += war_ts_core_matrix[i][j];
				//printf("%0.2lf", ts_core_matrix[i][j] + ts_core_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	fclose(fp);
	war_ts_core_volume = total;
}

	void 
dump_war_as_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX];
	long timeprint = (long) clock();
	sprintf(file_name, "%s/%s-%ld-war_as_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );
	fp = fopen (file_name, "w+");
	//printf("as_matrix_size: %d\n", as_matrix_size);
	double total = 0;
	//printf("all sharing matrix:\n");
	for(int i = as_matrix_size; i >= 0; i--)
	{
		for (int j = 0; j <= as_matrix_size; j++)
		{
			if(j < as_matrix_size) {
				fprintf(fp, "%0.2lf,", war_as_matrix[i][j] + war_as_matrix[j][i]);
				total += war_as_matrix[i][j];
				//printf("%0.2lf,", as_matrix[i][j] + as_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", war_as_matrix[i][j] + war_as_matrix[j][i]);
				total += war_as_matrix[i][j];
				//printf("%0.2lf", as_matrix[i][j] + as_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	war_as_volume = total;
	fclose(fp);
}

	void
dump_war_as_core_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX];
	long timeprint = (long) clock();
	sprintf(file_name, "%s/%s-%ld-war_as_core_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );
	fp = fopen (file_name, "w+");
	//printf("as_matrix_size: %d\n", as_matrix_size);
	double total = 0;
	//printf("all sharing matrix:\n");
	for(int i = as_core_matrix_size; i >= 0; i--)
	{
		for (int j = 0; j <= as_core_matrix_size; j++)
		{
			if(j < as_core_matrix_size) {
				fprintf(fp, "%0.2lf,", war_as_core_matrix[i][j] + war_as_core_matrix[j][i]);
				total += war_as_core_matrix[i][j];
				//printf("%0.2lf,", as_core_matrix[i][j] + as_core_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", war_as_core_matrix[i][j] + war_as_core_matrix[j][i]);
				total += war_as_core_matrix[i][j];
				//printf("%0.2lf", as_core_matrix[i][j] + as_core_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	war_as_core_volume = total;
	war_cache_line_transfer = total;
	war_cache_line_transfer_millions = total/(1000000);
	war_cache_line_transfer_gbytes = total*64/(1024*1024*1024);
	fclose(fp);
}

	void
dump_invalidation_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX];
	long timeprint = (long) clock();
	sprintf(file_name, "%s/%s-%ld-invalidation_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );

	fp = fopen (file_name, "w+");
	//printf("as_matrix_size: %d\n", as_matrix_size);
	double total = 0;
	//printf("all sharing matrix:\n");
	for(int i = as_matrix_size; i >= 0; i--)
	{
		for (int j = 0; j <= as_matrix_size; j++)
		{
			if(j < as_matrix_size) {
				fprintf(fp, "%0.2lf,", invalidation_matrix[i][j] + invalidation_matrix[j][i]);
				total += invalidation_matrix[i][j];
				//printf("%0.2lf,", as_matrix[i][j] + as_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", invalidation_matrix[i][j] + invalidation_matrix[j][i]);
				total += invalidation_matrix[i][j];
				//printf("%0.2lf", as_matrix[i][j] + as_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	invalidation_volume = total;
	fclose(fp);
}

	void
dump_invalidation_core_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX];
	long timeprint = (long) clock();
	/////////////////////////////////// WAW
	sprintf(file_name, "%s/%s-%ld-invalidation_core_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );
	fp = fopen (file_name, "w+");
	//printf("as_matrix_size: %d\n", as_matrix_size);
	double total = 0;
	//printf("all sharing matrix:\n");
	for(int i = as_core_matrix_size; i >= 0; i--)
	{
		for (int j = 0; j <= as_core_matrix_size; j++)
		{
			if(j < as_core_matrix_size) {
				fprintf(fp, "%0.2lf,", invalidation_core_matrix[i][j] + invalidation_core_matrix[j][i]);
				total += invalidation_core_matrix[i][j];
				//printf("%0.2lf,", as_core_matrix[i][j] + as_core_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", invalidation_core_matrix[i][j] + invalidation_core_matrix[j][i]);
				total += invalidation_core_matrix[i][j];
				//printf("%0.2lf", as_core_matrix[i][j] + as_core_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	invalidation_core_volume = total;
	fclose(fp);
}

	void 
dump_waw_fs_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX]; 
	sprintf(file_name, "%s/%s-%ld-waw_fs_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );
	fp = fopen (file_name, "w+");
	//printf("fs_matrix_size: %d\n", fs_matrix_size);
	double total= 0;
	for(int i = fs_matrix_size; i >= 0; i--) 
	{
		for (int j = 0; j <= fs_matrix_size; j++)
		{
			if(j < fs_matrix_size) {
				fprintf(fp, "%0.2lf,", waw_fs_matrix[i][j] + waw_fs_matrix[j][i]);
				total += waw_fs_matrix[i][j];
				//printf("%0.2lf,", fs_matrix[i][j] + fs_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", waw_fs_matrix[i][j] + waw_fs_matrix[j][i]);
				total += waw_fs_matrix[i][j];
				//printf("%0.2lf", fs_matrix[i][j] + fs_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	fclose(fp);
	waw_fs_volume = total;
}

	void 
dump_waw_fs_core_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX];
	sprintf(file_name, "%s/%s-%ld-waw_fs_core_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );
	fp = fopen (file_name, "w+");
	//printf("fs_core_matrix_size: %d\n", fs_core_matrix_size);
	double total= 0;
	for(int i = fs_core_matrix_size; i >= 0; i--)
	{
		for (int j = 0; j <= fs_core_matrix_size; j++)
		{
			if(j < fs_core_matrix_size) {
				fprintf(fp, "%0.2lf,", waw_fs_core_matrix[i][j] + waw_fs_core_matrix[j][i]);
				total += waw_fs_core_matrix[i][j];
				//printf("%0.2lf,", fs_core_matrix[i][j] + fs_core_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", waw_fs_core_matrix[i][j] + waw_fs_core_matrix[j][i]);
				total += waw_fs_core_matrix[i][j];
				//printf("%0.2lf", fs_core_matrix[i][j] + fs_core_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	fclose(fp);
	waw_fs_core_volume = total;
}

	void 
dump_waw_ts_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX];
	sprintf(file_name, "%s/%s-%ld-waw_ts_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );
	fp = fopen (file_name, "w+");
	//printf("ts_matrix_size: %d\n", ts_matrix_size);
	double total = 0;
	for(int i = ts_matrix_size; i >= 0; i--) 
	{
		for (int j = 0; j <= ts_matrix_size; j++)
		{
			if(j < ts_matrix_size) {
				fprintf(fp, "%0.2lf,", waw_ts_matrix[i][j] + waw_ts_matrix[j][i]);
				total += waw_ts_matrix[i][j];
				//printf("%0.2lf,", ts_matrix[i][j] + ts_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", waw_ts_matrix[i][j] + waw_ts_matrix[j][i]);
				total += waw_ts_matrix[i][j];
				//printf("%0.2lf", ts_matrix[i][j] + ts_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	fclose(fp);
	waw_ts_volume = total;
}

	void 
dump_waw_ts_core_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX];
	sprintf(file_name, "%s/%s-%ld-waw_ts_core_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );
	fp = fopen (file_name, "w+");
	//printf("ts_core_matrix_size: %d\n", ts_core_matrix_size);
	double total = 0;
	for(int i = ts_core_matrix_size; i >= 0; i--)
	{
		for (int j = 0; j <= ts_core_matrix_size; j++)
		{
			if(j < ts_core_matrix_size) {
				fprintf(fp, "%0.2lf,", waw_ts_core_matrix[i][j] + waw_ts_core_matrix[j][i]);
				total += waw_ts_core_matrix[i][j];
				//printf("%0.2lf,", ts_core_matrix[i][j] + ts_core_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", waw_ts_core_matrix[i][j] + waw_ts_core_matrix[j][i]);
				total += waw_ts_core_matrix[i][j];
				//printf("%0.2lf", ts_core_matrix[i][j] + ts_core_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	fclose(fp);
	waw_ts_core_volume = total;
}

	void 
dump_waw_as_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX];
	long timeprint = (long) clock();
	sprintf(file_name, "%s/%s-%ld-waw_as_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );
	fp = fopen (file_name, "w+");
	//printf("as_matrix_size: %d\n", as_matrix_size);
	double total = 0;
	//printf("all sharing matrix:\n");
	for(int i = as_matrix_size; i >= 0; i--)
	{
		for (int j = 0; j <= as_matrix_size; j++)
		{
			if(j < as_matrix_size) {
				fprintf(fp, "%0.2lf,", waw_as_matrix[i][j] + waw_as_matrix[j][i]);
				total += waw_as_matrix[i][j];
				//printf("%0.2lf,", as_matrix[i][j] + as_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", waw_as_matrix[i][j] + waw_as_matrix[j][i]);
				total += waw_as_matrix[i][j];
				//printf("%0.2lf", as_matrix[i][j] + as_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	waw_as_volume = total;
	fclose(fp);
}

	void 
dump_waw_as_core_matrix()
{
	FILE * fp;
	char file_name[PATH_MAX];
	long timeprint = (long) clock();
	sprintf(file_name, "%s/%s-%ld-waw_as_core_matrix.csv", output_directory, hpcrun_files_executable_name(), getpid() );
	fp = fopen (file_name, "w+");
	///printf("as_matrix_size: %d\n", as_matrix_size);
	double total = 0.0;
	///printf("all sharing matrix:\n");
	for(int i = as_core_matrix_size; i >= 0; i--)
	{
		for (int j = 0; j <= as_core_matrix_size; j++)
		{
			if(j < as_core_matrix_size) {
				fprintf(fp, "%0.2lf,", waw_as_core_matrix[i][j] + waw_as_core_matrix[j][i]);
				total += waw_as_core_matrix[i][j];
				//printf("%0.2lf,", as_core_matrix[i][j] + as_core_matrix[j][i]);
			} else {
				fprintf(fp, "%0.2lf", waw_as_core_matrix[i][j] + waw_as_core_matrix[j][i]);
				total += waw_as_core_matrix[i][j];
				//printf("%0.2lf", as_core_matrix[i][j] + as_core_matrix[j][i]);
			}
		}
		fprintf(fp,"\n");
		//printf("\n");
	}
	waw_as_core_volume = total;
	waw_cache_line_transfer = total;
	waw_cache_line_transfer_millions = total/(1000000);
	waw_cache_line_transfer_gbytes = total*64/(1024*1024*1024);
	fclose(fp);
}

