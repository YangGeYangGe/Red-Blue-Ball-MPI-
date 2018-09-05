#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define RED "\x1B[31m"
#define GRED "\x1b[41m"
#define BLUE "\x1b[34m"
#define GBLUE "\x1b[44m"
#define GWHITE "\x1b[47m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define BLACK "\x1b[30m"

void board_init(int cell_size, int grid[cell_size][cell_size]);
int finished(int cell_size, int rs, int grid[rs][cell_size], int tile_size, int threshold, int contain_ghost_flag, int current_processor, int print_flag);
void red_movement(int r, int c, int grid[r][c]);
void blue_movement(int r, int c, int gridb[r][c]);
void sequential_computation(int n, int grids[n][n], int t, int c, int max_iters, int if_self_checking);
void print_gird(int cell_size, int tmpgrid[cell_size][cell_size], int tile_size, int threshold);

int main(int argc, char **argv) {

	if (argc != 5) {
		printf("Please enter correct input!\n");
		return 0;
	}

	int myid;
	int numprocs;
	MPI_Status status;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);
	int i, j;
	int n = atoi(argv[1]);
	int t = atoi(argv[2]);
	int c = atoi(argv[3]);
	int max_iters = atoi(argv[4]);
	
	int tile_size = n / t;
	//compare with max_iters
	int current_count = 0;
	int allreduce_variable = 0;
	int max_allreduce_variable = 0;

	int *recvbuf;
	recvbuf = (int *)malloc(n*n * sizeof(int));
	int *sendbuf;
	int sendcount;
	int displs[numprocs];
	int recvcounts[numprocs];
	// process 0:
	if (myid == 0) {
		int grid[n][n];
		// CREATE grid
		board_init(n, grid);
		if (numprocs == 1) {
			sequential_computation(n, grid, tile_size, c, max_iters, 0);
			print_gird(n, grid, tile_size, c);
		}
		else {
			//each process will hold a number of k rows of tiles
			//therefore, for k < (n/t)(n/t is the number of rows of tiles), each processor (get (n/t) / numprocs) rows of tile

			//check if finished first, if so, do nothing, just print the result(tile(s))
			int first_status = finished(n, n, grid, tile_size, c, 0, 0, 0);

			if (first_status == 1) {
				print_gird(n, grid, tile_size, c);
				printf("initial grid does not need to move\n");
			}
			int num_t_row_in_processor;
			int sending_count = 0;
			int current_row = 0;
			int num_of_row_in_first_processor;
			int m;
			//tell other processor, need to work
			MPI_Bcast(&first_status, 1, MPI_INT, 0, MPI_COMM_WORLD);

			int rows_numbers[numprocs];
			//numprocs <= num_t_r && 
			if (first_status != 1) {
				//number of processors <= num of tile row, all processors can get job 
				if (numprocs <= t) {
					//set rows number to each processor
					//send original subgrid to each processor
					if (t % numprocs != 0) {
						current_row += tile_size*(t / numprocs + 1);
					}
					else {
						current_row += tile_size * (t / numprocs);
					}
					num_of_row_in_first_processor = current_row;
					rows_numbers[0] = num_of_row_in_first_processor;

					for (m = 1; m < numprocs; m++) {
						//fisrt set num of tile row for each processors
						if ((t % numprocs != 0) && ((t % numprocs) >  m)) {
							num_t_row_in_processor = t / numprocs + 1;
						}
						else {
							num_t_row_in_processor = t / numprocs;
						}
						// send size information
						MPI_Send(&num_t_row_in_processor, 1, MPI_INT, m, 1, MPI_COMM_WORLD);

						rows_numbers[m] = tile_size*num_t_row_in_processor;

						//make 1D array for sending data
						int sending[num_t_row_in_processor * tile_size * n];
						sending_count = 0;
						//set sending array
						for (i = 0; i < (num_t_row_in_processor * tile_size); i++) {
							for (j = 0; j < n; j++) {
								sending[sending_count] = grid[current_row][j];
								sending_count++;
							}
							current_row++;
						}
						//send this 1D array
						MPI_Send(sending, (num_t_row_in_processor * tile_size * n), MPI_INT, m, 2, MPI_COMM_WORLD);
						//all slaves get their rows
					}
					
				}//numprocs > num of tile row, some processors may just wait and do nothing
				else if (numprocs > t) {
					//each working processor have tile row 1
					num_of_row_in_first_processor = tile_size;
					current_row = tile_size;
					num_t_row_in_processor = 1;
					rows_numbers[0] = tile_size;
					for (m = 1; m < numprocs; m++) {
						//e.g. processor 0, 1, 2, 3, 4, 5,  num of tile rows is 4, first 4 processors can get job, then 
						//when it comes to the fifth processor(have id 4) and after this processor, cannot get job
						if (m == t) {
							//0 means no job
							num_t_row_in_processor = 0;
						}
						//save work load information
						rows_numbers[m] = num_t_row_in_processor * tile_size;
						//send work load(tile row number) to other processors
						MPI_Send(&num_t_row_in_processor, 1, MPI_INT, m, 1, MPI_COMM_WORLD);
						//then send actual row to working processors
						if (num_t_row_in_processor != 0) {
							int sending[num_t_row_in_processor * tile_size * n];
							sending_count = 0;
							for (i = 0; i < (num_t_row_in_processor * tile_size); i++) {
								for (j = 0; j < n; j++) {
									sending[sending_count] = grid[current_row][j];
									sending_count++;
								}
								current_row++;
							}
							//send
							MPI_Send(sending, (num_t_row_in_processor * tile_size * n), MPI_INT, m, 2, MPI_COMM_WORLD);
						}
					}
				}
				printf("id 0 got %d row(s) of tile\n", num_of_row_in_first_processor/tile_size);
				//actual last row
				int mylast[n];
				int mylast_counter = 0;
				//actual first row
				int myfirst[n];
				int myfirst_counter = 0;
				//virtual first row
				int topghost[n];
				//virtual last row
				int bottomghost[n];
				//overall 2D array(contains real and virtual rows)
				int sub_grid[num_of_row_in_first_processor + 2][n];
				int sub_grid_counter = 0;

				//set real rows
				int row_c = 0, col_c = 0;
				for (row_c = 1; row_c <= num_of_row_in_first_processor; row_c++) {
					for (col_c = 0; col_c < n; col_c++) {
						
						sub_grid[row_c][col_c] = grid[row_c - 1][col_c];
					}
				}
				current_count = 0;

				while (max_allreduce_variable == 0 && current_count < max_iters) {
					//get real last row
					for (mylast_counter = 0; mylast_counter < n; mylast_counter++) {

						mylast[mylast_counter] = sub_grid[num_of_row_in_first_processor][mylast_counter];
					}
					//get real first row
					for (myfirst_counter = 0; myfirst_counter < n; myfirst_counter++) {
						myfirst[myfirst_counter] = sub_grid[1][myfirst_counter];
					}

					int real_last_processor = 0;
					//set real last processor
					if (numprocs > t) {
						//e.g. numprocs = 5 (0,1,2,3,4), n_t_r = 4, then the real last one would be processor(myid) 3
						real_last_processor = t - 1;
					}
					else {
						real_last_processor = (myid - 1 + numprocs) % numprocs;
					}
					//communicate with second row and real last row(send real last row to second row, receive top ghost from real last row)
					MPI_Sendrecv(mylast, n, MPI_INT, (myid + 1 + numprocs) % numprocs, 1, topghost, n, MPI_INT, real_last_processor, 1, MPI_COMM_WORLD, &status);
					//send real first row to real last row, receive bottom ghostt from second row 
					MPI_Sendrecv(myfirst, n, MPI_INT, real_last_processor, 1, bottomghost, n, MPI_INT, (myid + 1 + numprocs) % numprocs, 1, MPI_COMM_WORLD, &status);

					//set top and bottom ghost in 2D array
					for (sub_grid_counter = 0; sub_grid_counter < n; sub_grid_counter++) {
						sub_grid[0][sub_grid_counter] = topghost[sub_grid_counter];
						sub_grid[num_of_row_in_first_processor + 1][sub_grid_counter] = bottomghost[sub_grid_counter];
					}
					//red movement
					red_movement(num_of_row_in_first_processor + 2, n, sub_grid);
					////then blue movement
					blue_movement(num_of_row_in_first_processor + 2, n, sub_grid);
					//increase current count, will compare with max_iters in the beginning of the loop
					current_count++;
					//if finished in processor 0
					if (finished(n, num_of_row_in_first_processor + 2, sub_grid, tile_size, c, 1, 0, 0) == 1) {
						allreduce_variable = 1;
					}
					// collect all if finished data
					MPI_Allreduce(&allreduce_variable, &max_allreduce_variable, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
					//if finished, break the loop, the movement part is over, do self-checking part
					if (max_allreduce_variable == 1) {
						break;
					}

				}
				printf("movement count is %d\n", current_count);
				//gather all result from other processors
				sendbuf = (int *)malloc(num_of_row_in_first_processor*n * sizeof(int));
				int sendbuf_count = 0;
				sendcount = num_of_row_in_first_processor*n;
				for (i = 1; i <= num_of_row_in_first_processor; i++) {
					for (j = 0; j < n; j++) {					
						sendbuf[sendbuf_count] = sub_grid[i][j];
						//printf("%d\n", sendbuf[sendbuf_count]);
						sendbuf_count++;
					}
				}
				//recvcounts, displs
				int current_displs = 0;
				for (i = 0; i < numprocs; i++) {
					displs[i] = current_displs;
					current_displs += rows_numbers[i]*n;
					recvcounts[i] = rows_numbers[i] * n;
				}
				
				MPI_Gatherv(sendbuf, sendcount, MPI_INT, recvbuf, recvcounts, displs, MPI_INT, 0, MPI_COMM_WORLD);
				//self_checking
				sequential_computation(n, grid, tile_size, c, max_iters, 1);
				int para_result[n][n];
				int compare_flag = 0;
				int compare_count = 0;

				for (i = 0; i < n; i++) {
					for (j = 0; j < n; j++) {
						para_result[i][j] = recvbuf[compare_count];
						compare_count++;
					}
				}
				print_gird(n, para_result, tile_size, c);

				for (i = 0; i < n; i ++) {
					for (j = 0; j < n; j++) {
						if (para_result[i][j] != grid[i][j]) {
							compare_flag = 1;
							printf("error! in row number %d, column number %d, sequential result is %d, parallel result is %d\n", i, j, grid[i][j], para_result[i][j]);
						}
						
					}
				}
				if (compare_flag == 0) {
					printf("campared with sequential result, all good!\n");
				}
			}

		}

	}
	else { // all other processes:
		   ////create a sub-grid from process 0.
		int first_status;
		//get first status from processor 0
		MPI_Bcast(&first_status, 1, MPI_INT, 0, MPI_COMM_WORLD);
		//need work
		if (first_status != 1) {
			int num_t_row_in_processor;
			//receive number of tile row from processor 0
			MPI_Recv(&num_t_row_in_processor, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
			printf("id %d got %d row(s) of tile\n", myid,num_t_row_in_processor);
			//if got job
			if (num_t_row_in_processor != 0) {
				int receive[num_t_row_in_processor* tile_size * n];
				//receive actual rows
				MPI_Recv(receive, (num_t_row_in_processor * tile_size * n), MPI_INT, 0, 2, MPI_COMM_WORLD, &status);
				i = 0;
				j = 0;
				// got initial and correct data
				int mylast[n];
				int mylast_counter = 0;
				int myfirst[n];
				int myfirst_counter = 0;
				int topghost[n];
				int bottomghost[n];
				//create 2D grid for movement
				int mygrid[num_t_row_in_processor*tile_size + 2][n];
				int mygrid_counter = 0;
				int row_c = 0, col_c = 0;
				int receive_counter = 0;
				//intialize the array with real data
				for (row_c = 1; row_c <= num_t_row_in_processor*tile_size; row_c++) {
					for (col_c = 0; col_c < n; col_c++) {
						mygrid[row_c][col_c] = receive[receive_counter];
						receive_counter++;
					}
				}
				while (max_allreduce_variable == 0 && current_count < max_iters) {
					//set real last array to send 
					for (mylast_counter = 0; mylast_counter < n; mylast_counter++) {
						mylast[mylast_counter] = mygrid[num_t_row_in_processor*tile_size][mylast_counter];
					}

					//set real first array to send
					for (myfirst_counter = 0; myfirst_counter < n; myfirst_counter++) {
						myfirst[myfirst_counter] = mygrid[1][myfirst_counter];
					}

					int real_next_processor = 0;
					//set real next procssor
					if (numprocs > t && (myid == t - 1)) {
						real_next_processor = 0;
					}
					else {
						real_next_processor = (myid + 1 + numprocs) % numprocs;
					}
					//communicate
					MPI_Sendrecv(mylast, n, MPI_INT, real_next_processor, 1, topghost, n, MPI_INT, (myid - 1 + numprocs) % numprocs, 1, MPI_COMM_WORLD, &status);
					MPI_Sendrecv(myfirst, n, MPI_INT, (myid - 1 + numprocs) % numprocs, 1, bottomghost, n, MPI_INT, real_next_processor, 1, MPI_COMM_WORLD, &status);
					//set ghost row in 2D array
					for (mygrid_counter = 0; mygrid_counter < n; mygrid_counter++) {
						mygrid[0][mygrid_counter] = topghost[mygrid_counter];
						mygrid[num_t_row_in_processor*tile_size + 1][mygrid_counter] = bottomghost[mygrid_counter];
					}
					//red movement
					red_movement(num_t_row_in_processor*tile_size + 2, n, mygrid);
					//blue movement
					blue_movement(num_t_row_in_processor*tile_size + 2, n, mygrid);
					//current count increase
					current_count++;
					//check if finished in this processor
					if (finished(n, num_t_row_in_processor*tile_size + 2, mygrid, tile_size, c, 1, myid, 0) == 1) {
						allreduce_variable = 1;
					}
					//check if finished in all processors
					MPI_Allreduce(&allreduce_variable, &max_allreduce_variable, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
					if (max_allreduce_variable == 1) {
						break;
					}
				}//
				 // self checking
				//int sendbuf[num_t_row_in_processor*tile_size * n];
				sendbuf = (int *)malloc(num_t_row_in_processor* tile_size * n * sizeof(int));
				int sendbuf_count = 0;
				sendcount = num_t_row_in_processor* tile_size * n;
				for (i = 1; i <= num_t_row_in_processor*tile_size; i++) {
					for (j = 0; j < n; j++) {
						sendbuf[sendbuf_count] = mygrid[i][j];		
						sendbuf_count++;
					}			
				}
				MPI_Gatherv(sendbuf, sendcount, MPI_INT, recvbuf, recvcounts, displs, MPI_INT, 0, MPI_COMM_WORLD);		
			}
			else {
				//the processors that do not have a job!
				//num_t_row_in_processor == 0, just waiting
				int current_count = 0;
				while (max_allreduce_variable == 0 && current_count < max_iters) {
					allreduce_variable = 0;
					MPI_Allreduce(&allreduce_variable, &max_allreduce_variable, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
					current_count++;
				}		
				MPI_Gatherv(sendbuf, 0, MPI_INT, recvbuf, recvcounts, displs, MPI_INT, 0, MPI_COMM_WORLD);			
			}
		}
	}
	MPI_Finalize();
	return 0;
}
//swap and randomize are from: https://www.geeksforgeeks.org/shuffle-a-given-array/
void swap(int *a, int *b) {
	int temp = *a;
	*a = *b;
	*b = temp;
}
void randomize(int arr[], int n)
{
	srand(time(NULL));
	int i = 0;
	for (i = n - 1; i > 0; i--)
	{
		int j = rand() % (i + 1);
		swap(&arr[i], &arr[j]);
	}
}

//set almost 1/3 red, 1/3 blue and 1/3 white 1D array, then shuffle and change it to 2D array
void board_init(int cell_size, int grid[cell_size][cell_size]) {
	int arraysize = cell_size*cell_size;
	int tmp[arraysize];
	int i, j;
	for (i = 0; i < arraysize; i++) {
		if (i < arraysize / 3) {
			tmp[i] = 1;
		}
		else if (i < 2 * arraysize / 3) {
			tmp[i] = 2;
		}
		else {
			tmp[i] = 0;
		}
	}
	randomize(tmp, arraysize);
	int count = 0;
	for (i = 0; i < cell_size; i++) {
		for (j = 0; j < cell_size; j++) {
			grid[i][j] = tmp[count];
			count++;
		}
	}
}
//check if finished, need add more things
int finished(int cell_size, int rs, int grid[rs][cell_size], int tile_size, int threshold, int contain_ghost_flag, int current_processor, int print_flag) {
	//change grid unit from cell to tile, then grid can be seen as (cell_size / tile_size) by (rs / tile_size).
	int cs = cell_size / tile_size;
	int rss = rs / tile_size;
	int count = 0; //will be (cell_size*cell_size)/(tile_size*tile_size)
	int red_count = 0;
	int blue_count = 0;
	int a, b;
	//flag indicate if need to ignore the first row and last row because of ghost rows
	if (contain_ghost_flag == 1) {
		rss = (rs - 2) / tile_size;
	}
	int if_finished_result = 0;
	for (a = 0; a < rss; a++) {
		for (b = 0; b < cs; b++) {
			//actual row number
			int row = 0;
			if (contain_ghost_flag == 1) {
				//ignore the first row as it is ghost
				row = a*tile_size + 1;
			}
			else {
				row = a*tile_size;
			}
			int col = b*tile_size;
			red_count = 0;
			blue_count = 0;
			//calculate red, blue number in a tile
			int a1, b1;
			for (a1 = row; a1 < row + tile_size; a1++) {
				for (b1 = col; b1 < col + tile_size; b1++) {
					if (grid[a1][b1] == 1) {
						red_count++;
					}
					else if (grid[a1][b1] == 2) {
						blue_count++;
					}
				}
			}
			count++;
			int c_a = 0;
			int c_b = 0;
			if (((double)red_count * 100 / (tile_size*tile_size)) >  (double)threshold) {
				if (print_flag == 0) {
					printf("id %d: (%d,%d) %f red finished\n", current_processor, a, b, (double)red_count * 100 / (tile_size*tile_size));
				}

				if_finished_result = 1;
			}

			if (((double)blue_count * 100 / (tile_size*tile_size)) >  (double)threshold) {
				if (print_flag == 0) {
					printf("id %d:  (%d,%d) %f blue finished\n", current_processor, a, b, (double)blue_count * 100 / (tile_size*tile_size));
				}
				if_finished_result = 1;
			}
		}

	}
	
	return if_finished_result;
}
//similar to finished, create a grid for printing 
void print_gird(int cell_size,int tmpgrid[cell_size][cell_size], int tile_size, int threshold) {
	int grid[cell_size][cell_size];
	int a, b;
	int count = 0;
	for (a = 0; a < cell_size; a++) {
		for (b = 0; b < cell_size; b++) {
			grid[a][b] = tmpgrid[a][b];
			
		}
	}
	int cs = cell_size / tile_size;
	int rss = cell_size / tile_size;
	count = 0; 
	int red_count = 0;
	int blue_count = 0;

	int if_finished_result;
	for (a = 0; a < rss; a++) {
		for (b = 0; b < cs; b++) {
			
			int row = 0;
			row = a*tile_size;	
			int col = b*tile_size;
			red_count = 0;
			blue_count = 0;
			int a1, b1;

			if_finished_result = 0;

			for (a1 = row; a1 < row + tile_size; a1++) {
				for (b1 = col; b1 < col + tile_size; b1++) {
					if (grid[a1][b1] == 1) {
						red_count++;
					}
					else if (grid[a1][b1] == 2) {
						blue_count++;
					}
				}
			}
			count++;
			int c_a = 0;
			int c_b = 0;
			if (((double)red_count * 100 / (tile_size*tile_size)) >  (double)threshold) {
				if_finished_result = 1;
			}
			if (((double)blue_count * 100 / (tile_size*tile_size)) >  (double)threshold) {
				if_finished_result = 1;
			}

			for (c_a = row; c_a < row + tile_size; c_a++) {

				for (c_b = col; c_b < col + tile_size; c_b++) {

					if (if_finished_result == 1) {
						if (grid[c_a][c_b] == 1) {
							grid[c_a][c_b] = 3;

						}
						else if (grid[c_a][c_b] == 2) {
							//printf(GBLUE"1 "ANSI_COLOR_RESET);
							grid[c_a][c_b] = 4;
						}
						else {
							grid[c_a][c_b] = 5;
						}
						
					}

				}
				
			}
		}
	}
	for (a = 0; a < cell_size; a++) {
		for (b = 0; b < cell_size; b++) {
			
			if (grid[a][b] == 1) {
				printf(RED"1 "ANSI_COLOR_RESET);
			}
			else if (grid[a][b] == 2) {
				printf(BLUE"2 "ANSI_COLOR_RESET);
			}
			else if (grid[a][b] == 3) {
				printf(GRED"1 "ANSI_COLOR_RESET);
			}
			else if (grid[a][b] == 4) {
				printf(GBLUE"2 "ANSI_COLOR_RESET);
			}
			else if (grid[a][b] == 0) {
				printf("0 ");
			}else if(grid[a][b] == 5) {
				printf(BLACK GWHITE"0 "ANSI_COLOR_RESET);
			}
			//printf("%d ", grid[a][b]);
			
		}
		printf("\n");
	}



}

void red_movement(int r, int c, int gridr[r][c]) {
	int i, j;
	int n = c;
	for (i = 0; i < r; i++) {

		if (gridr[i][0] == 1 && gridr[i][1] == 0) {
			gridr[i][0] = 4;
			gridr[i][1] = 3;
		}
		for (j = 1; j < c; j++) {
			if (gridr[i][j] == 1 && (gridr[i][(j + 1) % n] == 0)) {
				gridr[i][j] = 0;
				gridr[i][(j + 1) % n] = 3;
			}
			else if (gridr[i][j] == 3) {
				gridr[i][j] = 1;
			}
		}

		if (gridr[i][0] == 3) {
			gridr[i][0] = 1;
		}
		else if (gridr[i][0] == 4) {
			gridr[i][0] = 0;
		}
	}
}
//last blue cell cannot move to top cell
void blue_movement(int r, int c, int gridb[r][c]) {
	int i, j;
	int n = c;
	for (j = 0; j < n; j++) {
		if (gridb[0][j] == 2 && gridb[1][j] == 0) {

			gridb[0][j] = 4;
			gridb[1][j] = 3;
		}

		for (i = 1; i < r; i++) {

			if (gridb[i][j] == 2) {
				if ((i != r - 1) && (gridb[(i + 1)][j] == 0)) {				
					gridb[i][j] = 0;
					gridb[(i + 1)][j] = 3;
				}
			}
			else if (gridb[i][j] == 3) {
				gridb[i][j] = 2;
			}
		}
		if (gridb[0][j] == 3) {
			gridb[0][j] = 2;
		}
		else if (gridb[0][j] == 4) {
			gridb[0][j] = 0;
		}
	}
}
void sequential_computation(int n, int grids[n][n], int tile_size, int c, int max_iters, int if_self_checking) {
	int first_status = finished(n, n, grids, tile_size, c, 0, 0, 0);
	if (first_status == 1) {
		printf("initial grid does not need to move\n");
	}
	else {
		int count = 0;
		int i = 0;
		int j = 0;
		while (count < max_iters) {
			//normal red movement
			red_movement(n, n, grids);
			//blue movement(move as a ring)
			for (j = 0; j < n; j++) {
				if (grids[0][j] == 2 && grids[1][j] == 0) {
					grids[0][j] = 4;
					grids[1][j] = 3;
				}
				for (i = 1; i < n; i++) {
					if (grids[i][j] == 2 && (grids[(i + 1) % n][j] == 0)) {
						grids[i][j] = 0;
						grids[(i + 1) % n][j] = 3;
					}
					else if (grids[i][j] == 3) {
						grids[i][j] = 2;
					}
				}
				if (grids[0][j] == 3) {
					grids[0][j] = 2;
				}
				else if (grids[0][j] == 4) {
					grids[0][j] = 0;
				}
			}
			if (finished(n, n, grids, tile_size, c, 0, 0, if_self_checking) == 1) {
				if (if_self_checking == 0) {
					printf("movement count is %d\n", count);
				}
				break;
			}
			count++;
		}
	}	
}

