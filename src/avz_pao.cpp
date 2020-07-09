/*
 * @coding: utf-8
 * @Author: vector-wlc
 * @Date: 2020-02-06 10:22:46
 * @Description: old CLASS PaoOperator
 */

#include "libavz.h"

// 得到炮的恢复时间
int AvZ::PaoOperator::get_recover_time(int index)
{
	auto cannon = main_object->plantArray() + index;
	if (cannon->isDisappeared() || cannon->type() != YMJNP_47)
	{
		return -1;
	}
	auto animation_memory = pvz_base->animationMain()->animationOffset()->animationArray() + cannon->animationCode();

	switch (cannon->state())
	{
	case 35:
		return 125 + cannon->stateCountdown();
	case 36:
		return int(125 * (1 - animation_memory->circulationRate()) + 0.5) + 1;
	case 37:
		return 0;
	case 38:
		return 3125 + int(350 * (1 - animation_memory->circulationRate()) + 0.5);
	default:
		return -1;
	}
}

//获取屋顶炮飞行时间
int AvZ::PaoOperator::get_roof_fly_time(int pao_col, float drop_col)
{
	//得到落点对应的横坐标
	int drop_x = static_cast<int>(drop_col * 80);
	//得到该列炮最小飞行时间对应的最小的横坐标
	int min_drop_x = fly_time_data[pao_col - 1].min_drop_x;
	//得到最小的飞行时间
	int min_fly_time = fly_time_data[pao_col - 1].min_fly_time;
	//返回飞行时间
	return (drop_x >= min_drop_x ? min_fly_time : (min_fly_time + 1 - (drop_x - (min_drop_x - 1)) / 32));
}

void AvZ::PaoOperator::base_fire_pao(int pao_row, int pao_col, float drop_row, float drop_col)
{
	safeClick();

	for (int i = 0; i < 3; ++i)
	{
		clickGrid(pao_row, pao_col);
	}
	main_object->clickPaoCountdown() = 0;
	clickGrid(drop_row, drop_col);

	safeClick();
}

void AvZ::PaoOperator::delay_fire_pao(int delay_time, int pao_row, int pao_col, int row, float col)
{
	if (delay_time != 0)
	{
		// 将操作动态插入消息队列
		Grid grid = {pao_row, pao_col};
		lock_pao_set.insert(grid);
		setTime(nowTime(time_wave_run.wave) + delay_time,
				time_wave_run.wave);
		insertOperation([=]() {
			base_fire_pao(pao_row, pao_col, row, col);
			lock_pao_set.erase(grid);
		},
						"delay_fire_pao");
	}
	else
	{
		base_fire_pao(pao_row, pao_col, row, col);
	}
}

// 用户自定义炮位置发炮：单发
void AvZ::PaoOperator::rawPao(int pao_row, int pao_col, int drop_row, float drop_col)
{
	insertOperation([=]() {
		int index = getPlantIndex(pao_row, pao_col, YMJNP_47);
		if (index < 0)
		{
			popErrorWindowNotInQueue("请检查 (#, #) 是否为炮", pao_row, pao_col);
			return;
		}

		int recover_time = get_recover_time(index);
		if (recover_time > 0)
		{
			popErrorWindowNotInQueue("位于 (#, #) 的炮还有 # cs 恢复",
									 pao_row,
									 pao_col,
									 recover_time);
		}
		base_fire_pao(pao_row, pao_col, drop_row, drop_col);
	},
					"rawPao");
}

//用户自定义炮位置发炮：多发
void AvZ::PaoOperator::rawPao(const std::vector<PaoDrop> &lst)
{
	for (const auto &each : lst)
	{
		rawPao(each.pao_row, each.pao_col, each.drop_row, each.drop_col);
	}
}

//屋顶修正时间发炮，单发
void AvZ::PaoOperator::rawRoofPao(int pao_row, int pao_col, int drop_row, float drop_col)
{
	insertOperation([=]() {
		if (main_object->scene() != 4 && main_object->scene() != 5)
		{
			popErrorWindowNotInQueue("rawRoofPao : RawRoofPao函数只适用于RE与ME");
			return;
		}
		int index = getPlantIndex(pao_row, pao_col, YMJNP_47);
		if (index < 0)
		{
			popErrorWindowNotInQueue("请检查 (#, #) 是否为炮", pao_row, pao_col);
			return;
		}

		int recover_time = get_recover_time(index);
		int delay_time = 387 - get_roof_fly_time(pao_col, drop_col);
		if (recover_time > delay_time)
		{
			popErrorWindowNotInQueue("位于 (#, #) 的炮还有 # cs 恢复",
									 pao_row,
									 pao_col,
									 recover_time - delay_time);
			return;
		}
		delay_fire_pao(delay_time, pao_row, pao_col, drop_row, drop_col);
	},
					"rawRoofPao");
}

//屋顶修正时间发炮 多发
void AvZ::PaoOperator::rawRoofPao(const std::vector<PaoDrop> &lst)
{
	for (const auto &each : lst)
	{
		rawRoofPao(each.pao_row, each.pao_col, each.drop_row, each.drop_col);
	}
}

void AvZ::PaoOperator::plantPao(int row, int col)
{
	insertOperation([=]() {
		tick_runner.pushFunc([=]() {
			static int ymjnp_seed_index = getSeedIndex(YMJNP_47);
			static int ymts_seed_index = getSeedIndex(YMTS_34);
			static Seed *seed_memory;

			if (ymjnp_seed_index == -1 || ymts_seed_index == -1)
			{
				return;
			}

			for (int t_col = col; t_col < col + 2; ++t_col)
			{
				if (getPlantIndex(row, t_col, YMTS_34) != -1)
				{
					continue;
				}

				seed_memory = main_object->seedArray() + ymts_seed_index;
				if (!seed_memory->isUsable())
				{
					return;
				}

				cardNotInQueue(ymts_seed_index + 1, row, t_col);
			}

			seed_memory = main_object->seedArray() + ymjnp_seed_index;
			if (!seed_memory->isUsable())
			{
				return;
			}

			cardNotInQueue(ymjnp_seed_index + 1, row, col);
			setInsertOperation(false);
			tick_runner.stop();
			setInsertOperation(true);
		});
	},
					"plantPao");
}

AvZ::PaoOperator::PaoOperator()
{
	sequential_mode = TIME;
	next_pao = 0;
}

//用户重置炮列表
void AvZ::PaoOperator::resetPaoList(const std::vector<Grid> &lst)
{
	insertOperation([=]() {
		next_pao = 0;

		//重置炮列表
		pao_grid_vec = lst;
		getPlantIndexs(pao_grid_vec, YMJNP_47, pao_index_vec);
		auto pao_index_it = pao_index_vec.begin();
		auto pao_grid_it = pao_grid_vec.begin();
		while (pao_index_it != pao_index_vec.end())
		{
			if ((*pao_index_it) < 0)
			{
				popErrorWindowNotInQueue("resetPaoList : 请检查 (#, #) 位置是否为炮",
										 pao_grid_it->row,
										 pao_grid_it->col);
				return;
			}

			++pao_grid_it;
			++pao_index_it;
		}
	},
					"resetPaoList");
}

void AvZ::PaoOperator::setNextPao(int temp_next_pao)
{
	insertOperation([=]() {
		if (temp_next_pao > pao_grid_vec.size())
		{
			popErrorWindowNotInQueue("setNextPao : 本炮列表中一共有 # 门炮，您设的参数已溢出", pao_grid_vec.size());
			return;
		}
		next_pao = temp_next_pao - 1;
	},
					"setNextPao");
}

void AvZ::PaoOperator::setNextPao(int row, int col)
{
	insertOperation([=]() {
		int temp_next_pao = 0;
		Grid grid = {row, col};
		auto it = FindInAllRange(pao_grid_vec, grid);

		if (it != pao_grid_vec.end())
		{
			next_pao = it - pao_grid_vec.begin();
		}
		else
		{
			popErrorWindowNotInQueue("setNextPao : 请检查(#, #)是否在本炮列表中", row, col);
			return;
		}
	},
					"setNextPao");
}

void AvZ::PaoOperator::fixLatestPao()
{
	insertOperation([=]() {
		if (lastest_pao_msg.vec_index == -1)
		{
			popErrorWindowNotInQueue("fixLastPao ：您尚未使用炮");
			return;
		}
		lastest_pao_msg.is_writable = false; // 锁定信息
		int time = nowTime(time_wave_run.wave) + lastest_pao_msg.fire_time - main_object->gameClock() + 205;
		SetTime(time, time_wave_run.wave);
		insertOperation([=]() {
			lastest_pao_msg.is_writable = true; // 解锁信息
			shovelNotInQueue(pao_grid_vec[lastest_pao_msg.vec_index].row, pao_grid_vec[lastest_pao_msg.vec_index].col);
		});
		plantPao(pao_grid_vec[lastest_pao_msg.vec_index].row, pao_grid_vec[lastest_pao_msg.vec_index].col);
	},
					"fixLatestPao");
}

int AvZ::PaoOperator::get_recover_time_vec()
{
	int time = get_recover_time(pao_index_vec[next_pao]);
	if (time == -1)
	{
		int index = getPlantIndex(pao_grid_vec[next_pao].row, pao_grid_vec[next_pao].col, YMJNP_47);
		if (index < 0) // 找不到本来位置的炮
		{
			popErrorWindowNotInQueue("请检查位于 (#, #) 的第 # 门炮是否存在",
									 pao_grid_vec[next_pao].row,
									 pao_grid_vec[next_pao].col,
									 next_pao + 1);
			return -1;
		}
		pao_index_vec[next_pao] = index;
		time = get_recover_time(pao_index_vec[next_pao]);
	}
	return time;
}

int AvZ::PaoOperator::update_next_pao(int delay_time, bool is_recover_pao)
{
	int recover_time = 0xFFFF;

	if (sequential_mode == TIME)
	{
		int time;
		int _next_pao = next_pao;
		// 遍历整个炮列表
		for (int i = 0; i < pao_index_vec.size(); ++i, skip_pao(1))
		{
			// 被锁定的炮不允许发射
			if (lock_pao_set.find(pao_grid_vec[next_pao]) != lock_pao_set.end())
			{
				continue;
			}

			time = get_recover_time_vec();

			if (time == -1 || time <= delay_time)
			{
				return time;
			}

			if (recover_time > time)
			{
				recover_time = time;
				_next_pao = next_pao;
			}
		}

		next_pao = _next_pao;
	}
	else // SPACE
	{
		recover_time = get_recover_time_vec();
		if (recover_time == -1 || recover_time <= delay_time)
		{
			return recover_time;
		}
	}

	// 以上的判断条件已经解决炮是否存在以及炮当前时刻是否能用的问题
	// 如果炮当前时刻不能使用但是为 recoverPao 时则不会报错，
	// 并返回恢复时间
	if (is_recover_pao)
	{
		return recover_time;
	}

	std::string error_str = (sequential_mode == TIME ? "TIME 模式 : 未找到能够发射的炮，" : "SPACE 模式 : ");
	error_str += "位于 (#, #) 的第 # 门炮还有 #cs 恢复";
	popErrorWindowNotInQueue(error_str,
							 pao_grid_vec[next_pao].row,
							 pao_grid_vec[next_pao].col,
							 next_pao + 1,
							 recover_time - delay_time);
	return -1;
}

// 发炮函数：单发
void AvZ::PaoOperator::pao(int row, float col)
{
	insertOperation([=]() {
		if (pao_grid_vec.size() == 0)
		{
			popErrorWindowNotInQueue("pao : 您尚未为此炮列表分配炮");
			return;
		}
		if (update_next_pao() == -1)
		{
			return;
		}
		base_fire_pao(pao_grid_vec[next_pao].row, pao_grid_vec[next_pao].col, row, col);
		update_lastest_pao_msg(main_object->gameClock(), next_pao);
		skip_pao(1);
	},
					"pao");
}

//发炮函数：多发
void AvZ::PaoOperator::pao(const std::vector<Crood> &lst)
{
	for (const auto &each : lst)
	{
		pao(each.row, each.col);
	}
}

void AvZ::PaoOperator::recoverPao(int row, float col)
{
	insertOperation([=]() {
		if (pao_grid_vec.size() == 0)
		{
			popErrorWindowNotInQueue("recoverPao : 您尚未为此炮列表分配炮");
			return;
		}
		int delay_time = update_next_pao(0, true);
		if (delay_time == -1)
		{
			return;
		}
		delay_fire_pao(delay_time, pao_grid_vec[next_pao].row, pao_grid_vec[next_pao].col, row, col);
		update_lastest_pao_msg(main_object->gameClock() + delay_time, next_pao);
		skip_pao(1);
	},
					"recoverPao");
}

void AvZ::PaoOperator::recoverPao(const std::vector<Crood> &lst)
{
	for (const auto &each : lst)
	{
		recoverPao(each.row, each.col);
	}
}

void AvZ::PaoOperator::roofPao(int row, float col)
{
	insertOperation([=]() {
		if (main_object->scene() != 4 && main_object->scene() != 5)
		{
			popErrorWindowNotInQueue("roofPao : RoofPao函数只适用于 RE 与 ME ");
			return;
		}
		if (pao_grid_vec.size() == 0)
		{
			popErrorWindowNotInQueue("roofPao : 您尚未为此炮列表分配炮");
			return;
		}

		int delay_time = 387 - get_roof_fly_time(pao_grid_vec[next_pao].col, col);
		if (update_next_pao(delay_time) == -1)
		{
			return;
		}
		delay_fire_pao(delay_time, pao_grid_vec[next_pao].row, pao_grid_vec[next_pao].col, row, col);
		update_lastest_pao_msg(main_object->gameClock() + delay_time, next_pao);
		skip_pao(1);
	},
					"roofPao");
}

void AvZ::PaoOperator::roofPao(const std::vector<Crood> &lst)
{
	for (const auto &each : lst)
	{
		roofPao(each.row, each.col);
	}
}