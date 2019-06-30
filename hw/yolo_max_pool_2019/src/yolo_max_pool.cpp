#include "yolo_max_pool.h"
#include "float.h"
#include "ap_fixed.h"

void yolo_max_pool_top(yolo_stream_type &inStream, yolo_stream_type &outStream)
{
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS INTERFACE axis register both port=outStream
#pragma HLS INTERFACE axis register both port=inStream
	line_buff_type line_buff_group[INPUT_CHANNEL];
	window_type window_group[KERNEL_NUM];
#pragma HLS ARRAY_PARTITION variable=window_group complete dim=1
	float val_output[KERNEL_NUM];
#pragma HLS ARRAY_PARTITION variable=val_output complete dim=1
	float_32_side_channel curr_input;

	for(int out_row=0;out_row<OUTPUT_HEIGHT+POOL_PAD;out_row++)
	{
	for(int row_stride=0;row_stride<STRIDE;row_stride++)
	{
		for(int out_col=0;out_col<OUTPUT_WIDTH+POOL_PAD;out_col++)
		{
		for(int col_stride=0;col_stride<STRIDE;col_stride++)
		{
			for(int input_ch_idx=0;input_ch_idx<INPUT_CHANNEL;input_ch_idx++)
			{
#pragma HLS PIPELINE

				int row_idx = out_row*STRIDE+row_stride;
				int col_idx = out_col*STRIDE+col_stride;
				int conv_count;

				if((row_idx>=KERNEL_DIM-1)&&(col_idx>=KERNEL_DIM-1))
					conv_count = col_idx - (KERNEL_DIM-1);
				else
					conv_count = 0;


				if((row_idx>=(INPUT_HEIGHT-2*PAD))||(col_idx>=(INPUT_WIDTH-2*PAD)))
				{
					//padding
					curr_input.data = -FLT_MAX;
				}
				else
				{
					//stream input
					curr_input = inStream.read();
				}

				//line buffer for every input channel
				yolo_line_buffer(curr_input.data,&line_buff_group[input_ch_idx],col_idx);


				if((row_idx>=KERNEL_DIM-1)&&(col_idx>=KERNEL_DIM-1)&&(row_stride==STRIDE-1)&&(col_stride==STRIDE-1))
				{

					window_group[input_ch_idx] = slide_window(conv_count,&line_buff_group[input_ch_idx]);
					val_output[input_ch_idx] = window_max_pool(&window_group[input_ch_idx]);

					ap_uint<1> last;
					if((out_row==OUTPUT_HEIGHT+POOL_PAD-1)&&
					   (out_col==OUTPUT_WIDTH+POOL_PAD-1)&&
					   (input_ch_idx==INPUT_CHANNEL-1))
					{
						last = 1;
					}
					else
					{
						last = 0;
					}

					write_output(val_output[input_ch_idx],curr_input,outStream,last);
				}


			}
		}

	}

}
}}

void yolo_line_buffer(float curr_data, line_buff_type *line_buff, int col_idx)
{

	line_buff->shift_up(col_idx);
	line_buff->insert_top(curr_data,col_idx);

}

window_type slide_window(int conv_count, line_buff_type *line_buff)
{
	window_type kernel_window;

	for(int win_row=0; win_row < KERNEL_DIM; win_row++)
	{
		for(int win_col=0; win_col < KERNEL_DIM; win_col++)
		{
			float val = (float)line_buff->getval(win_row,win_col+conv_count);
			kernel_window.insert(val,win_row,win_col);
		}
	}

	return kernel_window;
}

float window_max_pool(window_type *window)
{
	float max = -FLT_MAX;
	for(int win_row=0; win_row < KERNEL_DIM; win_row++)
	{
		for(int win_col=0; win_col < KERNEL_DIM; win_col++)
		{
			float val = window->getval(win_row,win_col);
			if(val>max)
				max = val;
		}
	}
	return max;
}

void write_output(float val_output, float_32_side_channel curr_input, yolo_stream_type &out_stream, ap_uint<1> last)
{
	float_32_side_channel curr_output;

	curr_output.data = val_output;
	curr_output.keep = curr_input.keep;
	curr_output.strb = curr_input.strb;
	curr_output.user = curr_input.user;
	curr_output.last = last;
	curr_output.id = curr_input.id;
	curr_output.dest = curr_input.dest;

	out_stream.write(curr_output);
}
